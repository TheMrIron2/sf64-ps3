# extern

External dependencies live here.

Expected layout:

```text
extern/
  n64ps3/
```

Add `n64ps3` with:

```sh
git submodule add https://github.com/TheMrIron2/n64ps3 extern/n64ps3
```

After cloning this repository, initialize the existing submodule with:

```sh
git submodule update --init extern/n64ps3
```

Do not vendor ROMs, generated assets, or copyrighted game data here.
