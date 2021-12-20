#include <znc/User.h>
#include <znc/Modules.h>
#include <znc/znc.h>

#include <regex.h>
#include <vector>
#include <string>
#include <bitset>
#include <iostream>

const int NUM_MODES = 11;
const char MODES[NUM_MODES+1] = "mMaAnNcCjpq";

class Matcher {
protected:
	std::bitset<NUM_MODES> IgnoreModes;
public:
	Matcher(const CString& modes);
	virtual ~Matcher() {}

	CString Modes() const;
	std::string Bits() const;
	bool operator ==(const Matcher& other) const;

	virtual bool Match(CNick& nick, const CString& line, int mode) const = 0;
	virtual CString String() const = 0;
	virtual CString Data() const = 0;
	virtual CString Type() const = 0;
};

class HostMatcher : public Matcher {
protected:
	CString Mask;
public:
	HostMatcher(const CString& modes, const CString& mask);
	virtual ~HostMatcher() {}

	virtual bool Match(CNick& nick, const CString& line, int mode) const;
	virtual CString String() const;
	virtual CString Data() const;
	virtual CString Type() const;
};

class RegexMatcher : public Matcher {
protected:
	regex_t Regex;
	CString Pattern;
public:
	RegexMatcher(const CString& modes, const CString& re);
	~RegexMatcher();

	virtual bool Match(CNick& nick, const CString& line, int mode) const;
	virtual CString String() const;
	virtual CString Data() const;
	virtual CString Type() const;
};

// container because a std::vector<> can't contain my Matchers (abstract class
// cannot be used for templates)
typedef struct {
	Matcher* m;
} IgnoreEntry;

class ModIgnore : public CModule {
protected:
	std::vector<IgnoreEntry> IgnoreList;
public:
	virtual ~ModIgnore();
	virtual bool OnLoad(const CString& args, CString& message);

	// Commands exposed to the user
	void CmdAddHostMatcher(const CString& line);
	void CmdAddRegexMatcher(const CString& line);
	void CmdDelIgnore(const CString& line);
	void CmdList(const CString& line);
	void CmdClear(const CString& line);

	// ZNC hook overrides
	virtual EModRet OnChanMsg(CNick& nick, CChan& chan, CString& message);
	virtual EModRet OnPrivMsg(CNick& nick, CString& message);
	virtual EModRet OnChanAction(CNick& nick, CChan& chan, CString& message);
	virtual EModRet OnPrivAction(CNick& nick, CString& message);
	virtual EModRet OnChanNotice(CNick& nick, CChan& chan, CString& message);
	virtual EModRet OnPrivNotice(CNick& nick, CString& message);
	virtual EModRet OnChanCTCP(CNick& nick, CChan& chan, CString& message);
	virtual EModRet OnPrivCTCP(CNick& nick, CString& message);
	virtual EModRet OnRawMessage(CMessage &message);

	MODCONSTRUCTOR(ModIgnore) {
		AddHelpCommand();
		AddCommand("AddHost",	static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdAddHostMatcher),
			"[mMaAnNcCjpq] <nick!user@host>",	"Ignore a hostmask from [m]essage, [a]ction, [n]otice, [c]tcp, [j]oins, [p]arts, [q]uits; uppercase = private");
		AddCommand("AddPattern",static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdAddRegexMatcher),
			"[mMaAnNcCjpq] <regex>",			"Ignore text matching a regular expression");
		AddCommand("Del",		static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdDelIgnore),
			"<n>",							"Remove an ignore entry by index");
		AddCommand("List",		static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdList),
			"", 							"Display the ignore list");
		AddCommand("Clear",		static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdClear),
			"",								"Clear all ignore entries");
	}
private:
	void addIgnore(IgnoreEntry ignore);
	EModRet check(CNick& nick, CString& message, int mode);
	void cleanup();
};
