#include <znc/User.h>
#include <znc/Modules.h>
#include <znc/znc.h>

#include <regex.h>
#include <list>
#include <string>

using namespace std;

class Matcher {
protected:
	int IgnoreModes;
public:
	Matcher(CString modes);
	virtual ~Matcher() {}

	CString Modes() const;
	bool operator ==(Matcher& other);

	virtual bool Match(CNick& nick, CString& line, int mode) const = 0;
	virtual CString String() const = 0;
	virtual CString Data() const = 0;
	virtual CString Type() const = 0;
};

class HostMatcher : public Matcher {
protected:
	CString Mask;
public:
	HostMatcher(CString modes, CString mask);
	virtual ~HostMatcher() {}

	virtual bool Match(CNick& nick, CString& line, int mode) const;
	virtual CString String() const;
	virtual CString Data() const;
	virtual CString Type() const;
};

class RegexMatcher : public Matcher {
protected:
	regex_t Regex;
	CString Pattern;
public:
	RegexMatcher(CString modes, CString re);
	~RegexMatcher();

	virtual bool Match(CNick& nick, CString& line, int mode) const;
	virtual CString String() const;
	virtual CString Data() const;
	virtual CString Type() const;
};

// container because a std::list<> can't contain my Matchers (abstract class
// cannot be used for templates)
typedef struct {
	Matcher* m;
} IgnoreEntry;

class ModIgnore : public CModule {
protected:
	list<IgnoreEntry> IgnoreList;
public:
	virtual ~ModIgnore();
	virtual bool OnLoad(const CString& args, CString& message);

	void AddIgnore(IgnoreEntry ignore);

	// Commands exposed to the user
	void CmdAddHostMatcher(const CString& line);
	void CmdAddRegexMatcher(const CString& line);
	void CmdDelIgnore(const CString& line);
	void CmdUnload(const CString& line);
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

	MODCONSTRUCTOR(ModIgnore) {
		AddHelpCommand();
		AddCommand("AddHost",	static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdAddHostMatcher),
			"[mMaAnNcC] <nick!user@host>",	"Ignore a hostmask from [m]essage, [a]ction, [n]otice, [c]tcp; uppercase = private");
		AddCommand("AddPattern",static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdAddRegexMatcher),
			"[mMaAnNcC] <regex>",			"Ignore text matching a regular expression");
		AddCommand("Del",		static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdDelIgnore),
			"<n>",							"Remove an ignore entry by index");
		AddCommand("List",		static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdList),
			"", 							"Display the ignore list");
		AddCommand("Clear",		static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdClear),
			"",								"Clear all ignore entries");
		AddCommand("Unload",	static_cast<CModCommand::ModCmdFunc>(&ModIgnore::CmdUnload),
			"",								"Unload this module");
	}
private:
	EModRet check(CNick& nick, CString& message, int mode);
	void cleanup();
};
