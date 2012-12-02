An ignore module for ZNC.

Build with `znc-buildmod`. Move the resulting `.so` file to your `~/.znc/modules`.

Feel free to fork. I'll merge any pull requests that make sense. Perhaps some day I can get this into ZNC proper!

### Usage

```
+---------+-----------------------------+-------------------------------------------------------------------------------------+
| Command | Arguments                   | Description                                                                         |
+---------+-----------------------------+-------------------------------------------------------------------------------------+
[mMaAnNcC] | Ignore a hostmask. m, a, n, c = message, action, notice, CTCP; uppercase = private. |
| Clear   |                             | Clear all ignores from the list                                                     |
           | Unignore a hostmask                                                                 |
| Help    | search                      | Generate this output                                                                |
| List    |                             | Display the ignore list                                                             |
+---------+-----------------------------+-------------------------------------------------------------------------------------+
```