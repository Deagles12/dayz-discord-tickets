class CfgPatches
{
    class DZTickets
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
    };
};
class CfgMods
{
    class DZTickets
    {
        dir = "DZTickets";
        name = "DayZ Tickets";
        type = "mod";
        inputs = "DZTickets/inputs.xml";
        dependencies[] = {"Game", "Mission"};
        class defs
        {
            class gameScriptModule { value = ""; files[] = {"DZTickets/scripts/3_Game"}; };
            class missionScriptModule { value = ""; files[] = {"DZTickets/scripts/5_Mission"}; };
        };
    };
};
