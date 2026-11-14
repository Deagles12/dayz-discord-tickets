class DZTServerConfig
{
    ref array<string> admin_steam_ids = new array<string>;
    ref array<string> categories = {"Скарга", "Баг", "Підозра на чітера", "Інше"};
    int max_active = 3;
    int create_cooldown_seconds = 60;
    int reply_cooldown_seconds = 3;
    bool discord_enabled;
    string discord_webhook_url;
}

class DZTStoredEvent : DZTMessage
{
    int time_seconds;
    bool player_ack;
    string request_id;
    string request_op;
}

class DZTRecord
{
    int revision;
    int time_seconds;
    ref DZTTicket ticket;
    ref array<ref DZTStoredEvent> events = new array<ref DZTStoredEvent>;
}

class DZTStorage
{
    static const string ROOT = "$profile:DayZDiscordTickets";
    static const string DIRECTORY = "$profile:DayZDiscordTickets/tickets";
    ref map<int, ref DZTRecord> records = new map<int, ref DZTRecord>;
    ref array<int> ids = new array<int>;
    int lastEvent;
    int lastTicket;

    static int Days(int year, int month, int day)
    {
        // Gregorian calendar, March as first month to put leap day last.
        if (month <= 2)
        {
            year--;
            month += 12;
        }
        return 365 * year + year / 4 - year / 100 + year / 400 + (153 * (month - 3) + 2) / 5 + day - 1;
    }

    static int Now(out string stamp)
    {
        int year, month, day, hour, minute, second;
        GetYearMonthDayUTC(year, month, day);
        GetHourMinuteSecondUTC(hour, minute, second);
        stamp = string.Format("%1-%2-%3 %4:%5:%6 UTC", year, Pad(month), Pad(day), Pad(hour), Pad(minute), Pad(second));
        return (Days(year, month, day) - Days(2020, 1, 1)) * 86400 + hour * 3600 + minute * 60 + second;
    }

    static string Pad(int value)
    {
        if (value < 10) return "0" + value.ToString();
        return value.ToString();
    }

    string Path(int id, int slot)
    {
        return DIRECTORY + "/" + id.ToString() + "-" + slot.ToString() + ".json";
    }

    DZTRecord Read(string path)
    {
        DZTRecord record;
        string error;
        if (!JsonFileLoader<DZTRecord>.LoadFile(path, record, error)) return null;
        if (!record || !record.ticket || !record.events || record.events.Count() == 0 || record.revision < 1) return null;
        return record;
    }

    bool Load()
    {
        string name;
        FileAttr attributes;
        FindFileHandle handle = FindFile(DIRECTORY + "/*.json", name, attributes, FindFileFlags.DIRECTORIES);
        if (!handle) return true;
        bool valid = true;
        for (bool more = true; more; more = FindNextFile(handle, name, attributes))
        {
            int split = name.IndexOf("-");
            if (split <= 0) continue;
            int id = name.Substring(0, split).ToInt();
            if (id <= 0 || records.Contains(id)) continue;
            DZTRecord first = Read(Path(id, 0));
            DZTRecord second = Read(Path(id, 1));
            if (second && (!first || second.revision > first.revision)) first = second;
            if (!first || first.ticket.id != id)
            {
                Print("[DZTickets] Cannot load ticket #" + id.ToString());
                valid = false;
                continue;
            }
            records.Set(id, first);
            ids.Insert(id);
            if (id > lastTicket) lastTicket = id;
            foreach (DZTStoredEvent entry : first.events)
                if (entry.id > lastEvent) lastEvent = entry.id;
        }
        CloseFindFile(handle);
        ids.Sort();
        return valid;
    }

    bool Commit(DZTRecord record)
    {
        int revision = record.revision + 1;
        string path = Path(record.ticket.id, revision % 2);
        string data, error, written;
        record.revision = revision;
        bool saved = JsonFileLoader<DZTRecord>.MakeData(record, data, error, false);
        if (saved)
        {
            FileHandle file = OpenFile(path, FileMode.WRITE);
            saved = file != 0;
            if (saved)
            {
                FPrint(file, data);
                CloseFile(file);
                file = OpenFile(path, FileMode.READ);
                saved = file != 0;
                if (saved)
                {
                    ReadFile(file, written, data.Length() + 1);
                    CloseFile(file);
                    saved = written == data;
                }
                if (!saved) DeleteFile(path);
            }
        }
        if (!saved)
        {
            record.revision--;
            Print("[DZTickets] Ticket write failed.");
        }
        return saved;
    }
}



