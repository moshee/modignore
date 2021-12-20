#include <stdexcept>
#include "ignore.h"

using namespace std;

enum IMode {
	ModeMsg,
	ModePrivMsg,
	ModeAction,
	ModePrivAction,
	ModeNotice,
	ModePrivNotice,
	ModeCTCP,
	ModePrivCTCP,
	ModeJoin,
	ModePart,
	ModeQuit
};

class MatcherError : public runtime_error {
public:
	MatcherError(const string& err) : runtime_error(err) {}
};

Matcher::Matcher(const CString& input_modes) {
	if (input_modes.empty()) {
		IgnoreModes = bitset<NUM_MODES>(0);
	} else {
		int sz = (int)input_modes.length();
		char c0 = input_modes[0];

		if (c0 == '1' || c0 == '0') {
			// this means it's the third format revision
			// make use of bitset constructor
			string modes(input_modes);
			if (sz != NUM_MODES) {
				modes += string(NUM_MODES-sz, '0');
			}

			IgnoreModes = bitset<NUM_MODES>(modes);
		} else {
			// this means it's the second format revision
			// (man, I can imagine how code can get messy real fast trying to
			// support older versions)
			bool bad;

			for (int ch = 0; ch < sz; ch++) {
				bad = true;

				for (int mode = 0; mode < NUM_MODES; mode++) {
					if (MODES[mode] == input_modes[ch]) {
						IgnoreModes.set(mode);
						bad = false;
						break;
					}
				}
				if (bad) {
					// invalid character not found in MODES
					CString err("Invalid mode character: ");
					err.push_back(input_modes[ch]);
					throw MatcherError(err);
				}
			}
		}
	}

	if (IgnoreModes.count() == 0) {
		throw MatcherError("There are no modes for this ignore entry to act upon");
	}
}

// return mode letters (mMaAnNcC)
CString Matcher::Modes() const {
	CString s;

	for (int i = 0; i < NUM_MODES; i++) {
		if (IgnoreModes[i]) {
			s.push_back(MODES[i]);
		}
	}

	return s;
}

// return string representation of ignore modes bitmask for storage
string Matcher::Bits() const {
	return IgnoreModes.to_string();
}

bool Matcher::operator ==(const Matcher& other) const {
	return other.Data() == Data() && other.Type() == Type();
}

inline bool Matcher::Match(CNick& nick, const CString& line, int mode) const {
	return IgnoreModes[mode];
}

HostMatcher::HostMatcher(const CString& modes, const CString& mask) : Matcher(modes) {
	CNick host_tester(mask);
	CString host = host_tester.GetHostMask();

	if (host.Equals(mask)) {
		Mask = mask;
	} else {
		
		throw MatcherError("HostMatcher: Malformed hostmask.");
	}
}

bool HostMatcher::Match(CNick& nick, const CString& message, int mode) const {
	return Matcher::Match(nick, message, mode) && nick.GetHostMask().WildCmp(Mask);
}

CString HostMatcher::String() const {
	return "Hostmask: " + Mask + " [" + Matcher::Modes() + "]";
}

CString HostMatcher::Data() const {
	return Mask;
}

CString HostMatcher::Type() const {
	return "hostmask";
}

RegexMatcher::RegexMatcher(const CString& modes, const CString& re_string) : Matcher(modes) {
	const char* re = re_string.c_str();
	Pattern = re_string;

	int status = regcomp(&Regex, re, REG_EXTENDED|REG_NOSUB);
	if (status != 0) {
		size_t err_size = regerror(status, &Regex, NULL, 0);
		string error(err_size, '\0');
		(void)regerror(status, &Regex, &error[0], err_size);

		throw MatcherError("RegexMatcher: Failed to compile regular expression /" + re_string + "/: " + error);
	}
}

bool RegexMatcher::Match(CNick& nick, const CString& message, int mode) const {
	return Matcher::Match(nick, message, mode) && regexec(&Regex, message.c_str(), 0, NULL, 0) == 0;
}

CString RegexMatcher::String() const {
	return "Regex: /" + Pattern + "/ [" + Matcher::Modes() + "]";
}

CString RegexMatcher::Data() const {
	return Pattern;
}

CString RegexMatcher::Type() const {
	return CString("regex");
}

RegexMatcher::~RegexMatcher() {
	regfree(&Regex);
}

/*
 * Now it's time for the actual module!
 */

ModIgnore::~ModIgnore() {
	cleanup();
}

bool ModIgnore::OnLoad(const CString& args, CString& message) {
	for (MCString::iterator a = BeginNV(); a != EndNV(); ++a) {
		VCString parts;
		// modes|type
		size_t length = a->second.Split("|", parts);

		CString modes = parts[0];
		CString type;
		CString data = a->first;

		// for compatibility with the previous version, a value without a pipe
		// in it will be just the modes, in which case assume default of
		// hostmask type ignore.
		if (length == 1) {
			type = "hostmask";
			SetNV(data, modes + "|hostmask");
		} else {
			type = parts[1];
		}

		Matcher* m;

		try {
			if (type.Equals("hostmask")) {
				m = new HostMatcher(modes, data); // throws
			} else if (type.Equals("regex")) {
				m = new RegexMatcher(modes, data); // throws
			} else {
				message = "Invalid ignore type: '" + type + "'";
				return false;
			}
		} catch (exception& err) {
			// TODO: return false?
			cerr << "ignore: Error: " << err.what() << endl;
			DelNV(a);
			continue;
		}

		IgnoreEntry e = { m };
		IgnoreList.push_back(e);
	}

	size_t size = IgnoreList.size();
	if (size > 0) {
		message = CString((int)size) + " ignore" + (size == 1 ? "" : "s") + " loaded.";
	}
	return true;
}


void ModIgnore::addIgnore(IgnoreEntry ignore) {
	int i = 0;
	for (vector<IgnoreEntry>::iterator a = IgnoreList.begin(); a != IgnoreList.end(); ++a) {
		if (*ignore.m == *a->m) {
			PutModule("Error: the ignore:");
			PutModule("    " + a->m->String());
			PutModule("already exists as entry #" + CString(i) + ".");
			PutModule("To add the entry again (e.g. with different modes), first delete the existing entry.");
			return;
		}
		i++;
	}
	IgnoreList.push_back(ignore);
	SetNV(ignore.m->Data(), ignore.m->Bits() + "|" + ignore.m->Type());
	PutModule("Added " + ignore.m->String());
}

// /msg *ignore AddHost [mode chars] <hostmask>
void ModIgnore::CmdAddHostMatcher(const CString& line) {
	CString modes(MODES);
	CString mask;
	VCString tokens;

	int num = (int)line.QuoteSplit(tokens);

	switch (num) {
	case 0:
		// this should never happen
	case 1:
		{
		PutModule("Error: No hostmask specified.");
		return;
		}
	case 2:
		mask = tokens[1];
		break;
	default:
		{
		modes = tokens[1];
		mask = tokens[2];
		}
		break;
	}

	try {
		Matcher* m = new HostMatcher(modes, mask);
		IgnoreEntry e = { m };
		addIgnore(e);
	} catch (exception& err) {
		string msg("Error: ");
		msg += err.what();
		PutModule(msg);
		PutModule("The entry will not be added to the ignore list.");
	}
}

// /msg *ignore AddPattern [mode chars] <hostmask>
void ModIgnore::CmdAddRegexMatcher(const CString& line) {
	CString modes(MODES);
	CString re;
	VCString tokens;

	int num = (int)line.QuoteSplit(tokens);
	switch (num) {
	case 0:
		// this should never happen
	case 1:
		{
		PutModule("Error: No pattern specified.");
		return;
		}
	case 2:
		re = tokens[1];
		break;
	default:
		{
		modes = tokens[1];
		re = tokens[2];
		}
		break;
	}

	try {
		Matcher* m = new RegexMatcher(modes, re);
		IgnoreEntry e = { m };
		addIgnore(e);
	} catch (exception& err) {
		string msg("Error: ");
		msg += err.what();
		PutModule(msg);
		PutModule("The entry will not be added to the ignore list.");
	}
}

void ModIgnore::CmdDelIgnore(const CString& line) {
	CString arg = line.Token(1);
	if (arg.empty()) {
		PutModule("Error: No index given.");
		return;
	}

	int index = arg.ToInt();
	if (index > (int)IgnoreList.size()-1 || index < 0) {
		PutModule("Error: Invalid index.");
		return;
	}

	Matcher* m = IgnoreList[(size_t)index].m;
	
	// find the corresponding entry in the registry
	// we can't just use DelNV or FindNV directly because we have to see if the
	// type matches as well as the data
	for (MCString::iterator nv = BeginNV(); nv != EndNV(); ++nv) {
		VCString parts;

		// modes|type
		int size = (int)nv->second.Split("|", parts);
		CString type("hostmask");
		if (size == 2) {
			type = parts[1];
		}

		CString data = nv->first;

		if (type.Equals(m->Type()) && data.Equals(m->Data())) {
			DelNV(nv);
			CString s = m->String();
			IgnoreList.erase(IgnoreList.begin() + index);
			PutModule("Deleted " + s);
			return;
		}
	}
	// this should never happen
	PutModule("Error: Couldn't find corresponding ignore in the ZNC module registry.");
}

void ModIgnore::CmdList(const CString& line) {
	if (IgnoreList.empty()) {
		PutModule("Ignore list is empty.");
		return;
	}

	CTable table;
	table.AddColumn("#");
	table.AddColumn("Type");
	table.AddColumn("Data");
	table.AddColumn("Modes");

	int i = 0;
	for (vector<IgnoreEntry>::iterator ignore = IgnoreList.begin(); ignore != IgnoreList.end(); ++ignore) {
		table.AddRow();
		table.SetCell("#", CString(i));
		table.SetCell("Type", ignore->m->Type());
		table.SetCell("Data", ignore->m->Data());
		table.SetCell("Modes", ignore->m->Modes());
		i++;
	}

	PutModule(table);
	PutModule("(" + CString(IgnoreList.size()) + " entries)");
}

void ModIgnore::CmdClear(const CString& line) {
	if (!ClearNV()) {
		PutModule("Error: Failed to clear ZNC module registry.");
		return;
	}
	int size = IgnoreList.size();
	cleanup();
	PutModule(CString(size) + " ignore" + (size == 1 ? "" : "s") + " erased.");
}

CModule::EModRet ModIgnore::check(CNick& nick, CString& message, int mode) {
	for (vector<IgnoreEntry>::iterator ignore = IgnoreList.begin(); ignore != IgnoreList.end(); ++ignore) {
		if (ignore->m->Match(nick, message, mode)) {
			return HALT;
		}
	}
	return CONTINUE;
}

CModule::EModRet ModIgnore::OnRawMessage(CMessage &message) {
	CString msgtext;
	IMode msgmode;
	CMessage::Type mtype = message.GetType();

	if (mtype == CMessage::Type::Join) {
		msgmode = ModeJoin;
	} else if (mtype == CMessage::Type::Part) {
		msgmode = ModePart;
	} else if (mtype == CMessage::Type::Quit) {
		msgmode = ModeQuit;
	} else {
		return CONTINUE;
	}
	
	msgtext = message.ToString(message.IncludeAll);
	return check(message.GetNick(), msgtext, msgmode);
}

CModule::EModRet ModIgnore::OnChanMsg(CNick& nick, CChan& chan, CString& message) {
	return check(nick, message, ModeMsg);
}

CModule::EModRet ModIgnore::OnPrivMsg(CNick& nick, CString& message) {
	return check(nick, message, ModePrivMsg);
}

CModule::EModRet ModIgnore::OnChanAction(CNick& nick, CChan& chan, CString& message) {
	return check(nick, message, ModeAction);
}

CModule::EModRet ModIgnore::OnPrivAction(CNick& nick, CString& message) {
	return check(nick, message, ModePrivAction);
}

CModule::EModRet ModIgnore::OnChanNotice(CNick& nick, CChan& chan, CString& message) {
	return check(nick, message, ModeNotice);
}

CModule::EModRet ModIgnore::OnPrivNotice(CNick& nick, CString& message) {
	return check(nick, message, ModePrivNotice);
}

CModule::EModRet ModIgnore::OnChanCTCP(CNick& nick, CChan& chan, CString& message) {
	return check(nick, message, ModeCTCP);
}

CModule::EModRet ModIgnore::OnPrivCTCP(CNick& nick, CString& message) {
	return check(nick, message, ModePrivCTCP);
}

void ModIgnore::cleanup() {
	for (vector<IgnoreEntry>::iterator a = IgnoreList.begin(); a != IgnoreList.end(); ++a) {
		delete a->m;
	}
	IgnoreList.clear();
}

MODULEDEFS(ModIgnore, "Ignore lines by hostmask or text pattern");
