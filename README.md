# Icon Chance Randomizer

A Geometry Dash Geode mod for **Geode v5.10.1 / Geometry Dash 2.2081** that randomizes the cube using custom weights.

## Features

- Android64 build workflow for GitHub Actions
- In-game **Cube Chances** editor from the Garage
- Weighted random selection
- Vanilla cube IDs
- More Icons custom cube names
- Optional randomization on level initialization/retry
- Configuration is saved by Geode

## Pool format

Use comma-separated entries:

```text
vanilla:1=50,vanilla:7=10,custom:MyCube=40
```

Weights are relative, so that example produces 50%, 10%, and 40% selection probabilities.

### Vanilla cubes

```text
vanilla:1=50
vanilla:7=10
```

### More Icons cubes

```text
custom:MyCube=40
```

The `NAME` must be the More Icons icon name. The mod checks that the custom icon exists before adding it to the random pool.

## Build on GitHub

1. Create a GitHub repository.
2. Upload the contents of this folder to the repository root.
3. Open **Actions**.
4. The Android64 workflow builds the `.geode` package and uploads it as a workflow artifact.

The workflow uses Geode's official `geode-sdk/build-geode-mod` action with `sdk: given`, so the exact SDK version in `mod.json` is used.

## Dependencies

- Geode **v5.10.1**
- Geometry Dash **2.2081**
- More Icons **v2.1.3**

The project uses More Icons' documented `getIcon` / `setIcon` APIs for custom cube selection.
