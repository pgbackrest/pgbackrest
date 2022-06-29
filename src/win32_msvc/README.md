# Windows Compatibility

## Building dependencies

### `libyaml`

Follow steps documented [here](https://github.com/yaml/libyaml/issues/165).

### `libregex`

Download [sources](http://gnuwin32.sourceforge.net/downlinks/regex-src-zip.php) from the [gnuwin32](http://gnuwin32.sourceforge.net/packages/regex.htm) project.

Follow the following steps:

- Extract the downloaded files

- Copy `.h` and `.c` to [regex](./regex) folder.

- Change line 2897 in [regexec.c](./regex/regexec.c) from:

  ```c
  str_idx = path->next_idx ?: top_str;
  ```

  to

  ```c
  str_idx = path->next_idx ? path->next_idx : top_str;
  ```

- The custom [meson](./regex/meson.build) file will correctly compile the lib, and make available under variable name `dep_regex`.

- In the source [meson](../meson.build), line 404, it is declared as a dependency:

  ```meson
  lib_regex2 = declare_dependency(link_with : dep_regex)
  ```

### `libxml`

- Clone [libxml2](https://github.com/GNOME/libxml2) repo.

- Build with cmake:

```Powershell
cmake -S . -B libxml2-build -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=D:\Dep -D LIBXML2_WITH_ICONV=OFF -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_PYTHON=OFF

cmake --build ./libxml2-build --config Release

cmake --install libxml2-build
```

## Meson commands

```
meson configure --cmake-prefix-path <PATH> --backend vs .\vsbuild~
```
