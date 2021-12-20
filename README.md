An ignore module for ZNC ≥1.0.

Pull requests welcome.

### Install

```
$ znc-buildmod ignore.cc
$ mv ignore.so ~/.znc/modules
```

### Usage

	 Command    | Arguments                        | Description
	------------|----------------------------------|----------------------------------------------------------------------------
	 AddHost    | [mMaAnNcCjpq] `<nick!user@host>` | Ignore a hostmask from [m]essage, [a]ction, [n]otice, [c]tcp, [j]oins,
	            |                                  |         [p]arts, [q]uits, uppercase = private
	 AddPattern | [mMaAnNcCjpq] `<regex>`          | Ignore text matching a regular expression
	 Clear      |                                  | Clear all ignore entries
	 Del        | `<n>`                            | Remove an ignore entry by index
	 Help       | search                           | Generate this output
	 List       |                                  | Display the ignore list

The required arguments for AddHost and AddPattern should be "quoted" if they contain spaces.

### To do

- Ignore invites
- Ignore DCC (I'll have to take a look at how this works in the IRC protocol)
