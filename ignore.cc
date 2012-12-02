#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/znc.h>
#include <set>
#ifdef __DEBUG
#include <iostream>
#endif

using namespace std;

const int NUM_MODES = 8;

typedef enum {
	ModeMsg,
	ModePrivMsg,
	ModeAction,
	ModePrivAction,
	ModeNotice,
	ModePrivNotice,
	ModeCTCP,
	ModePrivCTCP,
} IgnoreMode;

static char ModeChars[NUM_MODES+1] = "mMaAnNcC";

// stackoverflow tells me to use a struct in the typedef instead of just using
// the array straight up
typedef struct IgnoreModes {
	bool modes[NUM_MODES];

	// Default constructor turns all modes on
	IgnoreModes() {
		for (int i = 0; i < NUM_MODES; i++) {
			modes[i] = true;
		}
	}

	IgnoreModes(CString sModes) {
		const char* cModes = sModes.c_str();
		// convert the string into a bool[NUM_MODES] by seeing which characters
		// are there
		for (int j = 0; j < NUM_MODES; j++) {
			for (unsigned int i = 0; i < sModes.length(); i++) {
				if (cModes[i] == ModeChars[j]) {
					modes[j] = true;
					goto next;
				}
			}
			modes[j] = false;
next:
			continue;
		}
	}

	CString String() {
		char c[NUM_MODES];
		int ptr = 0;
		for (int i = 0; i < NUM_MODES; i++) {
			if (modes[i]) {
				c[ptr++] = ModeChars[i];
			}
		}
		return CString(c, (size_t)ptr);
	}

#ifdef __DEBUG
	CString DebugString() {
		CString s("{ ");
		for (int i = 0; i < NUM_MODES; i++) {
			if (modes[i]) {
				s += "true";
			} else {
				s += "false";
			}
			s += " ";
		}
		s += "}";
		return s;
	}
#endif
} IgnoreModes;

typedef struct IgnoreEntry {
	CString mask;
	IgnoreModes modes;

	IgnoreEntry(const CString& _mask, IgnoreModes _modes)
		: mask(_mask)
		, modes(_modes) {}

	bool operator<(const IgnoreEntry& rhs) const {
		return mask.StrCmp(rhs.mask) == -1;
	}
} IgnoreEntry;

class CIgnore : public CModule {
public:
	void Ignore(CString mask, IgnoreModes modes) {
		IgnoreEntry entry(mask, modes);
#ifdef __DEBUG
		PutModule("[debug] Ignore(): modes is " + modes.DebugString());
#endif

		// have to do this manually for the case insensitive match
		for (set<IgnoreEntry>::iterator ignore = ignoreList.begin(); ignore != ignoreList.end(); ++ignore) {
			if (mask.Equals(ignore->mask)) {
				ignoreList.erase(ignore);
				ignoreList.insert(entry);
				PutModule("Updated '" + entry.mask + "' [" + entry.modes.String() + "].");
				return;
			}
		}
		// TODO: possible minor refactor opportunity here
		ignoreList.insert(entry);
		PutModule("Added '" + entry.mask + "' [" + entry.modes.String() + "] to ignore list.");
	}

	// syntax is like:
	//   /msg *ignore Ignore *!*@* [mode chars]
	// the first bit being the hostmask and the second bit being a string of
	// characters representing flags (top of the file has more info). If no
	// modes are given, all of them are used.
	void IgnoreCommand(const CString& line) {
		CString mask = line.Token(1);
		if (mask.empty()) {
			PutModule("Error: no hostmask specified");
			return;
		}
		CString sModes = line.Token(2);
		if (sModes.empty()) {
			sModes = CString(ModeChars);
			// if no modes were given then default to all ignore
			Ignore(mask, IgnoreModes());
		} else {
			Ignore(mask, IgnoreModes(sModes));
		}

#ifdef __DEBUG
		PutModule("[debug] sModes = " + sModes);
#endif

		SetNV(mask, sModes);
	}

	void UnignoreCommand(const CString& line) {
		CString mask = line.Token(1);
		for (set<IgnoreEntry>::iterator ignore = ignoreList.begin(); ignore != ignoreList.end(); ++ignore) {
			if (ignore->mask.Equals(mask)) {
				ignoreList.erase(ignore);
				DelNV(mask);
				PutModule("Erased '" + mask + "' from ignore list.");
				return;
			}
		}
		PutModule("Error: '" + mask + "' is not in the ignore list.");
	}

	void ListCommand(const CString& line) {
		if (ignoreList.empty()) {
			PutModule("Ignore list is empty.");
			return;
		}

		CTable table;
		table.AddColumn("Hostmask");
		table.AddColumn("Modes");

		for (set<IgnoreEntry>::iterator ignore = ignoreList.begin(); ignore != ignoreList.end(); ++ignore) {
			table.AddRow();
			table.SetCell("Hostmask", ignore->mask);
			IgnoreModes m = ignore->modes;
			table.SetCell("Modes", m.String());
		}

		PutModule(table);
	}

	void ClearCommand(const CString& line) {
		int size = ignoreList.size();
		ignoreList.clear();
		ClearNV();
		// just in case it fails or something... do I really need the '- ignoreList.size()' part?
		PutModule(CString(size - ignoreList.size()) + " ignore(s) erased.");
	}

#ifdef __DEBUG
	void SizeCommand(const CString& line) {
		PutModule(CString((int)ignoreList.size()));
	}

	void RegCommand(const CString& line) {
		CTable table;
		table.AddColumn("Hostmask");
		table.AddColumn("Modes");

		for (MCString::iterator a = BeginNV(); a != EndNV(); ++a) {
			table.AddRow();
			table.SetCell("Hostmask", a->first);
			table.SetCell("Modes", a->second);
		}

		PutModule(table);
	}
#endif

	MODCONSTRUCTOR(CIgnore) {
		AddHelpCommand();
		AddCommand("Add",	static_cast<CModCommand::ModCmdFunc>(&CIgnore::IgnoreCommand),
				"<nick!user@host> [mMaAnNcC]",	"Ignore a hostmask. m, a, n, c = message, action, notice, CTCP; uppercase = private.");
		AddCommand("Del",	static_cast<CModCommand::ModCmdFunc>(&CIgnore::UnignoreCommand),
				"<nick!user@host>",				"Unignore a hostmask");
		AddCommand("List",	static_cast<CModCommand::ModCmdFunc>(&CIgnore::ListCommand),
				"", 							"Display the ignore list");
		AddCommand("Clear",	static_cast<CModCommand::ModCmdFunc>(&CIgnore::ClearCommand),
				"",								"Clear all ignores from the list");
#ifdef __DEBUG
		AddCommand("Size",	static_cast<CModCommand::ModCmdFunc>(&CIgnore::SizeCommand),
				"",								"(debug) Get # of ignores");
		AddCommand("Reg",	static_cast<CModCommand::ModCmdFunc>(&CIgnore::RegCommand),
				"",								"(debug) List registry");
#endif
	}

	virtual ~CIgnore() {}

	virtual bool OnLoad(const CString& args, CString& message) {
		for (MCString::iterator a = BeginNV(); a != EndNV(); ++a) {
#ifdef __DEBUG
			cerr << ">>>>>> " << a->first << " " << a->second << endl;
#endif
			IgnoreModes modes(a->second);
			Ignore(a->first, modes);
		}

		int size = (int)ignoreList.size();
		if (size > 0) message = CString(size) + " ignore(s) loaded.";
#ifdef __DEBUG
		message = "[DEBUG MODE] " + message;
#endif

		return true;
	}

	// this is the main point of the module: checks if the sender's hostmask
	// matches any of the ones in the list and sends a HALT if it does
	EModRet Check(CNick& nick, IgnoreMode mode) {
		CString mask = nick.GetHostMask();
		for (set<IgnoreEntry>::iterator ignore = ignoreList.begin(); ignore != ignoreList.end(); ++ignore) {
			if (!ignore->modes.modes[mode]) continue;
			if (mask.WildCmp(ignore->mask)) {
#ifdef __DEBUG
				PutModule("[debug] Ignoring line matching '"
						+ ignore->mask + "' ["
						+ CString(ModeChars[mode]) + "] from '"
						+ mask + "'.");
#endif
				return HALT;
			}
		}
		return CONTINUE;
	}

	virtual EModRet OnChanMsg(CNick& nick, CChan& chan, CString& message) {
		return Check(nick, ModeMsg);
	}

	virtual EModRet OnPrivMsg(CNick& nick, CString& message) {
		return Check(nick, ModePrivMsg);
	}

	virtual EModRet OnChanAction(CNick& nick, CChan& chan, CString& message) {
		return Check(nick, ModeAction);
	}

	virtual EModRet OnPrivAction(CNick& nick, CString& message) {
		return Check(nick, ModePrivAction);
	}

	virtual EModRet OnChanNotice(CNick& nick, CChan& chan, CString& message) {
		return Check(nick, ModeNotice);
	}

	virtual EModRet OnPrivNotice(CNick& nick, CString& message) {
		return Check(nick, ModePrivNotice);
	}

	virtual EModRet OnChanCTCP(CNick& nick, CChan& chan, CString& message) {
		return Check(nick, ModeCTCP);
	}

	virtual EModRet OnPrivCTCP(CNick& nick, CString& message) {
		return Check(nick, ModePrivCTCP);
	}

private:
	set<IgnoreEntry> ignoreList;
};

USERMODULEDEFS(CIgnore, "ignore users by hostmask")
