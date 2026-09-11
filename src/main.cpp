#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace geode::prelude;
using namespace more_icons;

namespace icr {

struct PoolEntry {
    enum class Kind {
        Vanilla,
        Custom
    };

    Kind kind = Kind::Vanilla;
    int id = 1;
    std::string name;
    double weight = 1.0;
};

std::string trim(std::string value) {
    auto isSpace = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    value.erase(
        value.begin(),
        std::find_if(
            value.begin(),
            value.end(),
            [&](char c) {
                return !isSpace(static_cast<unsigned char>(c));
            }
        )
    );

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            [&](char c) {
                return !isSpace(static_cast<unsigned char>(c));
            }
        ).base(),
        value.end()
    );

    return value;
}

bool parseInt(std::string_view text, int& out) {
    if (text.empty()) {
        return false;
    }

    auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        out
    );

    return result.ec == std::errc{} &&
           result.ptr == text.data() + text.size();
}

bool parseWeight(std::string_view text, double& out) {
    std::string owned(text);

    if (owned.empty()) {
        return false;
    }

    char* end = nullptr;

    out = std::strtod(owned.c_str(), &end);

    return end != owned.c_str() &&
           *end == '\0' &&
           std::isfinite(out) &&
           out > 0.0;
}

std::vector<PoolEntry> parsePool(std::string const& raw) {
    std::vector<PoolEntry> result;

    std::size_t start = 0;

    while (start <= raw.size()) {
        auto comma = raw.find(',', start);

        auto token = trim(
            raw.substr(
                start,
                comma == std::string::npos
                    ? std::string::npos
                    : comma - start
            )
        );

        if (!token.empty()) {
            auto equals = token.rfind('=');

            if (equals == std::string::npos) {
                log::warn(
                    "ICR: ignored '{}': missing '='",
                    token
                );
            }
            else {
                auto left = trim(token.substr(0, equals));
                auto weightText = trim(token.substr(equals + 1));

                double weight = 0.0;

                if (!parseWeight(weightText, weight)) {
                    log::warn(
                        "ICR: ignored '{}': invalid weight",
                        token
                    );
                }
                else if (left.rfind("vanilla:", 0) == 0) {
                    int id = 0;

                    auto idText = trim(left.substr(8));

                    if (!parseInt(idText, id) || id <= 0) {
                        log::warn(
                            "ICR: ignored '{}': invalid vanilla cube ID",
                            token
                        );
                    }
                    else {
                        result.push_back(
                            PoolEntry{
                                PoolEntry::Kind::Vanilla,
                                id,
                                {},
                                weight
                            }
                        );
                    }
                }
                else if (left.rfind("custom:", 0) == 0) {
                    auto name = trim(left.substr(7));

                    if (name.empty()) {
                        log::warn(
                            "ICR: ignored '{}': empty More Icons name",
                            token
                        );
                    }
                    else {
                        result.push_back(
                            PoolEntry{
                                PoolEntry::Kind::Custom,
                                0,
                                std::move(name),
                                weight
                            }
                        );
                    }
                }
                else {
                    log::warn(
                        "ICR: ignored '{}': use vanilla:ID or custom:NAME",
                        token
                    );
                }
            }
        }

        if (comma == std::string::npos) {
            break;
        }

        start = comma + 1;
    }

    return result;
}

bool enabled() {
    return Mod::get()->getSettingValue<bool>("enabled");
}

void applyEntry(PoolEntry const& entry) {
    auto gm = GameManager::get();

    if (entry.kind == PoolEntry::Kind::Vanilla) {
        gm->setPlayerFrame(entry.id);

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
    if (!enabled()) {
        return;
    }

    auto raw =
        Mod::get()->getSettingValue<std::string>("pool");

    auto parsed = parsePool(raw);

    if (parsed.empty()) {
        log::warn(
            "ICR: pool is empty or contains no valid entries"
        );

        return;
    }

    std::vector<PoolEntry const*> valid;
    std::vector<double> weights;

    valid.reserve(parsed.size());
    weights.reserve(parsed.size());

    for (auto const& entry : parsed) {
        if (
            entry.kind == PoolEntry::Kind::Custom &&
            !more_icons::getIcon(
                entry.name,
                IconType::Cube
            )
        ) {
            log::warn(
                "ICR: skipping missing More Icons cube '{}'",
                entry.name
            );

            continue;
        }

        valid.push_back(&entry);
        weights.push_back(entry.weight);
    }

    if (valid.empty()) {
        log::warn(
            "ICR: no usable entries remain in the pool"
        );

        return;
    }

    static std::random_device rd;
    static std::mt19937_64 rng(rd());

    std::discrete_distribution<std::size_t> dist(
        weights.begin(),
        weights.end()
    );

    applyEntry(*valid[dist(rng)]);
}

void savePool(std::string const& pool) {
    Mod::get()->setSettingValue(
        "pool",
        pool
    );
}

std::vector<std::string> getMoreIconsCubes() {
    std::vector<std::string> result;

    auto* icons = more_icons::getIcons(
        IconType::Cube
    );

    if (!icons) {
        return result;
    }

    result.reserve(icons->size());

    for (auto const& icon : *icons) {
        result.push_back(icon.getName());
    }

    return result;
}

} // namespace icr


struct SelectorRow {
    bool custom = false;
    std::size_t index = 0;

    CCLabelBMFont* selected = nullptr;
    CCMenuItemSpriteExtra* typeButton = nullptr;
    TextInput* chance = nullptr;

    std::vector<std::string> customNames;
};


class CubeChancePopup final : public geode::Popup {

protected:
    std::vector<SelectorRow> m_rows;

    bool setup(std::string const&) {
        this->setTitle("Cube Chances");

        auto description = CCLabelBMFont::create(
            "Choose an icon and give it a chance weight.",
            "goldFont.fnt"
        );

        description->setScale(0.42f);

        m_mainLayer->addChildAtPosition(
            description,
            Anchor::Top,
            ccp(0.f, -28.f)
        );

        for (int i = 0; i < 5; ++i) {
            createRow(i);
        }

        auto saveSprite = ButtonSprite::create(
            "Save"
        );

        auto saveButton =
            CCMenuItemSpriteExtra::create(
                saveSprite,
                this,
                menu_selector(
                    CubeChancePopup::onSave
                )
            );

        saveButton->setID("save-button");

        m_buttonMenu->addChildAtPosition(
            saveButton,
            Anchor::Bottom,
            ccp(-75.f, 18.f)
        );

        auto saveRollSprite = ButtonSprite::create(
            "Save + Roll"
        );

        auto saveRollButton =
            CCMenuItemSpriteExtra::create(
                saveRollSprite,
                this,
                menu_selector(
                    CubeChancePopup::onSaveAndRandomize
                )
            );

        saveRollButton->setID(
            "save-roll-button"
        );

        m_buttonMenu->addChildAtPosition(
            saveRollButton,
            Anchor::Bottom,
            ccp(75.f, 18.f)
        );

        return true;
    }

    void createRow(int rowIndex) {
        float y =
            105.f -
            static_cast<float>(rowIndex) * 43.f;

        SelectorRow row;

        row.custom = false;
        row.index = static_cast<std::size_t>(
            rowIndex
        );

        row.customNames =
            icr::getMoreIconsCubes();

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

        row.typeButton->setTag(rowIndex);

        row.typeButton->setID(
            fmt::format(
                "type-button-{}",
                rowIndex
            )
        );

        m_buttonMenu->addChildAtPosition(
            row.typeButton,
            Anchor::Center,
            ccp(-145.f, y)
        );

        auto leftSprite =
            ButtonSprite::create(
                "<"
            );

        auto leftButton =
            CCMenuItemSpriteExtra::create(
                leftSprite,
                this,
                menu_selector(
                    CubeChancePopup::onLeft
                )
            );

        leftButton->setTag(rowIndex);

        leftButton->setID(
            fmt::format(
                "left-button-{}",
                rowIndex
            )
        );

        m_buttonMenu->addChildAtPosition(
            leftButton,
            Anchor::Center,
            ccp(-82.f, y)
        );

        row.selected =
            CCLabelBMFont::create(
                "1",
                "bigFont.fnt"
            );

        row.selected->setScale(0.42f);

        m_mainLayer->addChildAtPosition(
            row.selected,
            Anchor::Center,
            ccp(-20.f, y)
        );

        auto rightSprite =
            ButtonSprite::create(
                ">"
            );

        auto rightButton =
            CCMenuItemSpriteExtra::create(
                rightSprite,
                this,
                menu_selector(
                    CubeChancePopup::onRight
                )
            );

        rightButton->setTag(rowIndex);

        rightButton->setID(
            fmt::format(
                "right-button-{}",
                rowIndex
            )
        );

        m_buttonMenu->addChildAtPosition(
            rightButton,
            Anchor::Center,
            ccp(42.f, y)
        );

        row.chance =
            TextInput::create(
                70.f,
                "Chance",
                "bigFont.fnt"
            );

        row.chance->setLabel(
            "Chance"
        );

        row.chance->setFilter(
            "0123456789."
        );

        row.chance->setMaxCharCount(
            12
        );

        row.chance->setString(
            "1",
            false
        );

        m_mainLayer->addChildAtPosition(
            row.chance,
            Anchor::Center,
            ccp(125.f, y)
        );

        m_rows.push_back(
            std::move(row)
        );

        refreshRow(rowIndex);
    }

    std::string getChance(
        SelectorRow const& row
    ) const {
        /*
         * Geode v5 TextInput::getString() returns
         * gd::string on Android.
         *
         * Explicitly convert it to std::string here.
         */
        return std::string(
            row.chance->getString().c_str()
        );
    }

    void refreshRow(int rowIndex) {
        if (
            rowIndex < 0 ||
            rowIndex >= static_cast<int>(
                m_rows.size()
            )
        ) {
            return;
        }

        auto& row =
            m_rows[
                static_cast<std::size_t>(rowIndex)
            ];

        auto typeSprite =
            ButtonSprite::create(
                row.custom
                    ? "More Icons"
                    : "Vanilla"
            );

        row.typeButton->setNormalImage(
            typeSprite
        );

        if (row.custom) {
            if (row.customNames.empty()) {
                row.selected->setString(
                    "None"
                );
                return;
            }

            if (
                row.index >=
                row.customNames.size()
            ) {
                row.index = 0;
            }

            row.selected->setString(
                row.customNames[row.index]
                    .c_str()
            );
        }
        else {
            /*
             * Geometry Dash has a large number of
             * vanilla cubes. We keep the selector
             * within a practical range.
             */
            constexpr std::size_t VANILLA_COUNT =
                50;

            if (
                row.index >= VANILLA_COUNT
            ) {
                row.index = 0;
            }

            row.selected->setString(
                std::to_string(
                    row.index + 1
                ).c_str()
            );
        }
    }

    void cycleRow(
        int rowIndex,
        int direction
    ) {
        if (
            rowIndex < 0 ||
            rowIndex >= static_cast<int>(
                m_rows.size()
            )
        ) {
            return;
        }

        auto& row =
            m_rows[
                static_cast<std::size_t>(rowIndex)
            ];

        std::size_t count = 0;

        if (row.custom) {
            count = row.customNames.size();
        }
        else {
            count = 50;
        }

        if (count == 0) {
            return;
        }

        if (direction > 0) {
            row.index =
                (row.index + 1) % count;
        }
        else {
            if (row.index == 0) {
                row.index = count - 1;
            }
            else {
                --row.index;
            }
        }

        refreshRow(rowIndex);
    }

    void toggleType(int rowIndex) {
        if (
            rowIndex < 0 ||
            rowIndex >= static_cast<int>(
                m_rows.size()
            )
        ) {
            return;
        }

        auto& row =
            m_rows[
                static_cast<std::size_t>(rowIndex)
            ];

        row.custom = !row.custom;
        row.index = 0;

        if (row.custom) {
            row.customNames =
                icr::getMoreIconsCubes();
        }

        refreshRow(rowIndex);
    }

    std::string buildPool() const {
        std::string pool;

        for (
            std::size_t i = 0;
            i < m_rows.size();
            ++i
        ) {
            auto const& row = m_rows[i];

            std::string chance =
                getChance(row);

            if (chance.empty()) {
                continue;
            }

            if (!pool.empty()) {
                pool += ",";
            }

            if (row.custom) {
                if (
                    row.customNames.empty() ||
                    row.index >=
                        row.customNames.size()
                ) {
                    continue;
                }

                /*
                 * IMPORTANT:
                 * chance is already std::string,
                 * avoiding gd::string + std::string
                 * compilation errors on Android.
                 */
                pool +=
                    "custom:" +
                    row.customNames[row.index] +
                    "=" +
                    chance;
            }
            else {
                /*
                 * IMPORTANT:
                 * std::string conversion of
                 * TextInput::getString() happens
                 * before this concatenation.
                 */
                pool +=
                    "vanilla:" +
                    std::to_string(
                        row.index + 1
                    ) +
                    "=" +
                    chance;
            }
        }

        return pool;
    }

    void onType(CCObject* sender) {
        auto item =
            static_cast<CCMenuItemSpriteExtra*>(
                sender
            );

        toggleType(
            item->getTag()
        );
    }

    void onLeft(CCObject* sender) {
        auto item =
            static_cast<CCMenuItemSpriteExtra*>(
                sender
            );

        cycleRow(
            item->getTag(),
            -1
        );
    }

    void onRight(CCObject* sender) {
        auto item =
            static_cast<CCMenuItemSpriteExtra*>(
                sender
            );

        cycleRow(
            item->getTag(),
            1
        );
    }

    void onSave(CCObject*) {
        auto pool =
            buildPool();

        if (pool.empty()) {
            FLAlertLayer::create(
                "Icon Chance Randomizer",
                "No valid icon entries were entered.",
                "OK"
            )->show();

            return;
        }

        icr::savePool(
            pool
        );

        if (
            Mod::get()->getSettingValue<bool>(
                "debug"
            )
        ) {
            log::info(
                "ICR: saved pool '{}'",
                pool
            );
        }

        this->onClose(
            nullptr
        );
    }

    void onSaveAndRandomize(
        CCObject*
    ) {
        auto pool =
            buildPool();

        if (pool.empty()) {
            FLAlertLayer::create(
                "Icon Chance Randomizer",
                "No valid icon entries were entered.",
                "OK"
            )->show();

            return;
        }

        icr::savePool(
            pool
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
                480.f,
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
        if (!GJGarageLayer::init()) {
            return false;
        }

        auto menu =
            getChildByIDRecursive(
                "shards-menu"
            );

        if (!menu) {
            menu =
                getChildByIDRecursive(
                    "player-menu"
                );
        }

        if (!menu) {
            log::warn(
                "ICR: could not find garage menu"
            );

            return true;
        }

        auto buttonSprite =
            ButtonSprite::create(
                "Chances"
            );

        auto button =
            CCMenuItemSpriteExtra::create(
                buttonSprite,
                this,
                menu_selector(
                    IconChanceGarageLayer::onOpenChances
                )
            );

        button->setID(
            "icon-chance-randomizer-button"
        );

        static_cast<CCMenu*>(
            menu
        )->addChild(
            button
        );

        static_cast<CCMenu*>(
            menu
        )->updateLayout();

        return true;
    }

    void onOpenChances(CCObject*) {
        auto popup =
            CubeChancePopup::create();

        if (popup) {
            popup->show();
        }
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
