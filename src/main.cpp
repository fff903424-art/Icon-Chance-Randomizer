#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;
using namespace more_icons;

namespace icr {

struct PoolEntry {
    enum class Kind {
        Vanilla,
        Custom
    };

    Kind kind;
    int id = 0;
    std::string name;
    double weight = 0.0;
};

struct SelectorRow {
    bool custom = false;
    int index = 0;
    TextInput* chance = nullptr;
    CCLabelBMFont* selected = nullptr;
    CCMenuItemSpriteExtra* typeButton = nullptr;
};

bool parseWeight(std::string const& text, double& out) {
    if (text.empty())
        return false;

    char* end = nullptr;
    out = std::strtod(text.c_str(), &end);

    return end != text.c_str()
        && *end == '\0'
        && std::isfinite(out)
        && out > 0.0;
}

std::vector<PoolEntry> parsePool(std::string const& raw) {
    std::vector<PoolEntry> result;

    std::size_t start = 0;

    while (start <= raw.size()) {
        auto comma = raw.find(',', start);

        auto token = raw.substr(
            start,
            comma == std::string::npos
                ? std::string::npos
                : comma - start
        );

        if (!token.empty()) {
            auto equals = token.rfind('=');

            if (equals != std::string::npos) {
                auto left = token.substr(0, equals);
                auto weightText = token.substr(equals + 1);

                double weight = 0.0;

                if (parseWeight(weightText, weight)) {
                    if (left.rfind("vanilla:", 0) == 0) {
                        int id = 0;
                        auto idText = left.substr(8);

                        auto parsed = std::from_chars(
                            idText.data(),
                            idText.data() + idText.size(),
                            id
                        );

                        if (
                            parsed.ec == std::errc{} &&
                            parsed.ptr == idText.data() + idText.size() &&
                            id > 0
                        ) {
                            result.push_back({
                                PoolEntry::Kind::Vanilla,
                                id,
                                {},
                                weight
                            });
                        }
                    }
                    else if (left.rfind("custom:", 0) == 0) {
                        auto name = left.substr(7);

                        if (!name.empty()) {
                            result.push_back({
                                PoolEntry::Kind::Custom,
                                0,
                                name,
                                weight
                            });
                        }
                    }
                }
            }
        }

        if (comma == std::string::npos)
            break;

        start = comma + 1;
    }

    return result;
}

bool enabled() {
    return Mod::get()->getSettingValue<bool>("enabled");
}

void applyEntry(PoolEntry const& entry) {
    if (entry.kind == PoolEntry::Kind::Vanilla) {
        GameManager::get()->setPlayerFrame(entry.id);

        if (Mod::get()->getSettingValue<bool>("debug")) {
            log::info(
                "ICR: selected vanilla cube {}",
                entry.id
            );
        }

        return;
    }

    auto* icon = more_icons::getIcon(
        entry.name,
        IconType::Cube
    );

    if (!icon) {
        log::warn(
            "ICR: More Icons cube '{}' was not found",
            entry.name
        );
        return;
    }

    more_icons::setIcon(
        icon,
        IconType::Cube
    );

    if (Mod::get()->getSettingValue<bool>("debug")) {
        log::info(
            "ICR: selected More Icons cube '{}'",
            entry.name
        );
    }
}

void randomize() {
    if (!enabled())
        return;

    auto pool =
        Mod::get()->getSettingValue<std::string>("pool");

    auto parsed = parsePool(pool);

    if (parsed.empty()) {
        log::warn(
            "ICR: no valid icons in pool"
        );
        return;
    }

    std::vector<PoolEntry const*> valid;
    std::vector<double> weights;

    for (auto const& entry : parsed) {
        if (entry.kind == PoolEntry::Kind::Custom) {
            if (!more_icons::getIcon(
                entry.name,
                IconType::Cube
            )) {
                continue;
            }
        }

        valid.push_back(&entry);
        weights.push_back(entry.weight);
    }

    if (valid.empty())
        return;

    static std::random_device rd;
    static std::mt19937_64 rng(rd());

    std::discrete_distribution<std::size_t> distribution(
        weights.begin(),
        weights.end()
    );

    applyEntry(
        *valid[distribution(rng)]
    );
}

void savePool(std::string const& pool) {
    Mod::get()->setSettingValue(
        "pool",
        pool
    );
}

} // namespace icr


class CubeChancePopup final : public geode::Popup {
    std::vector<icr::SelectorRow> m_rows;

    std::vector<icr::PoolEntry> m_existing;

    static constexpr int ROW_COUNT = 5;

    int vanillaCount() const {
        return 50;
    }

    std::vector<std::string> getCustomNames() const {
        std::vector<std::string> names;

        auto* icons =
            more_icons::getIcons(
                IconType::Cube
            );

        if (!icons)
            return names;

        for (auto const& icon : *icons) {
            auto name = icon.getName();

            if (!name.empty())
                names.push_back(name);
        }

        return names;
    }

    double findWeight(
        bool custom,
        int index
    ) {
        if (custom) {
            auto names = getCustomNames();

            if (
                index < 0 ||
                index >= static_cast<int>(names.size())
            ) {
                return 0.0;
            }

            auto const& name = names[index];

            for (auto const& entry : m_existing) {
                if (
                    entry.kind ==
                    icr::PoolEntry::Kind::Custom &&
                    entry.name == name
                ) {
                    return entry.weight;
                }
            }

            return 0.0;
        }

        int id = index + 1;

        for (auto const& entry : m_existing) {
            if (
                entry.kind ==
                icr::PoolEntry::Kind::Vanilla &&
                entry.id == id
            ) {
                return entry.weight;
            }
        }

        return 0.0;
    }

    void setChanceText(
        TextInput* input,
        double weight
    ) {
        if (weight <= 0.0) {
            input->setString(
                "0",
                false
            );
            return;
        }

        char buffer[32];

        std::snprintf(
            buffer,
            sizeof(buffer),
            "%.4g",
            weight
        );

        input->setString(
            buffer,
            false
        );
    }

    void updateRow(
        int rowIndex
    ) {
        if (
            rowIndex < 0 ||
            rowIndex >= static_cast<int>(m_rows.size())
        ) {
            return;
        }

        auto& row = m_rows[rowIndex];

        if (row.custom) {
            auto names =
                getCustomNames();

            if (names.empty()) {
                row.index = 0;

                row.selected->setString(
                    "None"
                );

                return;
            }

            if (row.index >= static_cast<int>(names.size()))
                row.index = 0;

            if (row.index < 0)
                row.index =
                    static_cast<int>(names.size()) - 1;

            row.selected->setString(
                names[row.index].c_str()
            );

            row.selected->limitLabelWidth(
                105.f,
                0.32f
            );
        }
        else {
            if (row.index < 0)
                row.index = vanillaCount() - 1;

            if (row.index >= vanillaCount())
                row.index = 0;

            row.selected->setString(
                std::to_string(
                    row.index + 1
                ).c_str()
            );
        }

        if (row.typeButton) {
            auto sprite =
                ButtonSprite::create(
                    row.custom
                        ? "More Icons"
                        : "Vanilla"
                );

            row.typeButton->setNormalImage(
                sprite
            );
        }
    }

    void cycleRow(
        int rowIndex,
        int direction
    ) {
        if (
            rowIndex < 0 ||
            rowIndex >= static_cast<int>(m_rows.size())
        ) {
            return;
        }

        auto& row = m_rows[rowIndex];

        int count = 0;

        if (row.custom) {
            count =
                static_cast<int>(
                    getCustomNames().size()
                );
        }
        else {
            count = vanillaCount();
        }

        if (count <= 0)
            return;

        row.index += direction;

        if (row.index < 0)
            row.index = count - 1;

        if (row.index >= count)
            row.index = 0;

        updateRow(rowIndex);
    }

    void toggleType(
        int rowIndex
    ) {
        if (
            rowIndex < 0 ||
            rowIndex >= static_cast<int>(m_rows.size())
        ) {
            return;
        }

        auto& row = m_rows[rowIndex];

        double currentWeight = 0.0;

        parseWeight(
            row.chance->getString(),
            currentWeight
        );

        row.custom = !row.custom;
        row.index = 0;

        updateRow(rowIndex);

        auto newWeight =
            findWeight(
                row.custom,
                row.index
            );

        if (newWeight > 0.0)
            setChanceText(
                row.chance,
                newWeight
            );
        else
            setChanceText(
                row.chance,
                currentWeight
            );
    }

    std::string buildPool() {
        std::string pool;

        auto add = [&](std::string const& value) {
            if (!pool.empty())
                pool += ",";

            pool += value;
        };

        auto customNames =
            getCustomNames();

        for (auto const& row : m_rows) {
            double weight = 0.0;

            if (
                !parseWeight(
                    row.chance->getString(),
                    weight
                )
            ) {
                continue;
            }

            if (weight <= 0.0)
                continue;

            if (row.custom) {
                if (
                    row.index < 0 ||
                    row.index >=
                        static_cast<int>(
                            customNames.size()
                        )
                ) {
                    continue;
                }

                add(
                    "custom:" +
                    customNames[row.index] +
                    "=" +
                    row.chance->getString()
                );
            }
            else {
                add(
                    "vanilla:" +
                    std::to_string(
                        row.index + 1
                    ) +
                    "=" +
                    row.chance->getString()
                );
            }
        }

        return pool;
    }

    void createArrow(
        int rowIndex,
        int direction,
        float x,
        float y
    ) {
        auto sprite =
            ButtonSprite::create(
                direction < 0
                    ? "◂"
                    : "▸"
            );

        auto button =
            CCMenuItemSpriteExtra::create(
                sprite,
                this,
                menu_selector(
                    CubeChancePopup::onArrow
                )
            );

        button->setTag(
            rowIndex * 2 +
            (direction > 0 ? 1 : 0)
        );

        m_buttonMenu->addChildAtPosition(
            button,
            Anchor::Center,
            ccp(x, y)
        );
    }

protected:
    bool setup(
        std::string const&
    ) {
        this->setTitle(
            "Cube Chances"
        );

        auto pool =
            Mod::get()->getSettingValue<std::string>(
                "pool"
            );

        m_existing =
            icr::parsePool(pool);

        auto info =
            CCLabelBMFont::create(
                "Set the chance for each slot",
                "goldFont.fnt"
            );

        info->setScale(0.36f);

        m_mainLayer->addChildAtPosition(
            info,
            Anchor::Top,
            ccp(0, -32)
        );

        constexpr float startY = 62.f;
        constexpr float rowGap = 47.f;

        for (int i = 0; i < ROW_COUNT; ++i) {
            float y =
                startY -
                static_cast<float>(i) *
                    rowGap;

            auto& row =
                m_rows.emplace_back();

            row.custom = false;
            row.index = i;

            auto typeSprite =
                ButtonSprite::create(
                    "Vanilla"
                );

            row.typeButton =
                CCMenuItemSpriteExtra::create(
                    typeSprite,
                    this,
                    menu_selector(
                        CubeChancePopup::onType
                    )
                );

            row.typeButton->setTag(i);

            m_buttonMenu->addChildAtPosition(
                row.typeButton,
                Anchor::Center,
                ccp(-120, y)
            );

            auto leftSprite =
                ButtonSprite::create(
                    "◂"
                );

            auto left =
                CCMenuItemSpriteExtra::create(
                    leftSprite,
                    this,
                    menu_selector(
                        CubeChancePopup::onArrow
                    )
                );

            left->setTag(
                i * 2
            );

            m_buttonMenu->addChildAtPosition(
                left,
                Anchor::Center,
                ccp(-53, y)
            );

            auto selected =
                CCLabelBMFont::create(
                    "1",
                    "bigFont.fnt"
                );

            selected->setScale(0.34f);

            row.selected =
                selected;

            m_mainLayer->addChildAtPosition(
                selected,
                Anchor::Center,
                ccp(-20, y)
            );

            auto rightSprite =
                ButtonSprite::create(
                    "▸"
                );

            auto right =
                CCMenuItemSpriteExtra::create(
                    rightSprite,
                    this,
                    menu_selector(
                        CubeChancePopup::onArrow
                    )
                );

            right->setTag(
                i * 2 + 1
            );

            m_buttonMenu->addChildAtPosition(
                right,
                Anchor::Center,
                ccp(15, y)
            );

            auto chanceLabel =
                CCLabelBMFont::create(
                    "Chance:",
                    "bigFont.fnt"
                );

            chanceLabel->setScale(
                0.30f
            );

            m_mainLayer->addChildAtPosition(
                chanceLabel,
                Anchor::Center,
                ccp(68, y)
            );

            row.chance =
                TextInput::create(
                    60.f,
                    "0",
                    "bigFont.fnt"
                );

            row.chance->setLabel(
                ""
            );

            row.chance->setMaxCharCount(
                12
            );

            auto existing =
                findWeight(
                    row.custom,
                    row.index
                );

            setChanceText(
                row.chance,
                existing
            );

            m_mainLayer->addChildAtPosition(
                row.chance,
                Anchor::Center,
                ccp(123, y)
            );

            updateRow(i);
        }

        auto saveSprite =
            ButtonSprite::create(
                "Save"
            );

        auto save =
            CCMenuItemSpriteExtra::create(
                saveSprite,
                this,
                menu_selector(
                    CubeChancePopup::onSave
                )
            );

        m_buttonMenu->addChildAtPosition(
            save,
            Anchor::Bottom,
            ccp(-70, 17)
        );

        auto rollSprite =
            ButtonSprite::create(
                "Save + Roll"
            );

        auto roll =
            CCMenuItemSpriteExtra::create(
                rollSprite,
                this,
                menu_selector(
                    CubeChancePopup::onSaveAndRoll
                )
            );

        m_buttonMenu->addChildAtPosition(
            roll,
            Anchor::Bottom,
            ccp(70, 17)
        );

        return true;
    }

    void onArrow(
        CCObject* sender
    ) {
        auto button =
            static_cast<CCNode*>(sender);

        int tag =
            button->getTag();

        int row =
            tag / 2;

        int direction =
            tag % 2 == 0
                ? -1
                : 1;

        cycleRow(
            row,
            direction
        );
    }

    void onType(
        CCObject* sender
    ) {
        auto button =
            static_cast<CCNode*>(sender);

        toggleType(
            button->getTag()
        );
    }

    void onSave(
        CCObject*
    ) {
        icr::savePool(
            buildPool()
        );

        this->onClose(
            nullptr
        );
    }

    void onSaveAndRoll(
        CCObject*
    ) {
        icr::savePool(
            buildPool()
        );

        icr::randomize();

        this->onClose(
            nullptr
        );
    }

public:
    static CubeChancePopup* create() {
        auto ret =
            new CubeChancePopup();

        if (
            ret &&
            ret->init(
                420.f,
                330.f,
                "GJ_square01.png"
            )
        ) {
            ret->autorelease();

            if (
                !ret->setup("")
            ) {
                ret->release();
                return nullptr;
            }

            return ret;
        }

        delete ret;
        return nullptr;
    }
};


class $modify(
    IconChanceGarageLayer,
    GJGarageLayer
) {
    bool init() {
        if (
            !GJGarageLayer::init()
        )
            return false;

        auto menu =
            this->getChildByIDRecursive(
                "shards-menu"
            );

        if (!menu)
            menu =
                this->getChildByIDRecursive(
                    "player-menu"
                );

        auto buttonSprite =
            ButtonSprite::create(
                "Chances"
            );

        auto button =
            CCMenuItemSpriteExtra::create(
                buttonSprite,
                this,
                menu_selector(
                    IconChanceGarageLayer::openChances
                )
            );

        button->setID(
            "icon-chance-button"
        );

        if (menu) {
            static_cast<CCMenu*>(menu)
                ->addChild(button);

            static_cast<CCMenu*>(menu)
                ->updateLayout();
        }

        return true;
    }

    void openChances(
        CCObject*
    ) {
        CubeChancePopup::create()
            ->show();
    }
};


class $modify(
    IconChancePlayLayer,
    PlayLayer
) {
    bool init(
        GJGameLevel* level,
        bool useReplay,
        bool dontCreateObjects
    ) {
        if (
            !PlayLayer::init(
                level,
                useReplay,
                dontCreateObjects
            )
        ) {
            return false;
        }

        if (
            Mod::get()->getSettingValue<bool>(
                "randomize_on_restart"
            )
        ) {
            icr::randomize();
        }

        return true;
    }
};
