/*
 * mod-accountwide-flight-paths
 *
 * A flight path discovered by one character on an account is known to the account's other
 * characters the next time they log in, as long as they're of a faction that can use it.
 *
 * Released under the MIT License.
 */

#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <string>
#include <type_traits>
#include <utility>

namespace
{
    struct Config
    {
        bool enabled = true;
        bool announce = true;
        bool shareDeathKnightStartNodes = false;
    };

    Config config;

    // Taxi node ids start at 1; bit (id - 1) of the mask is node id.
    constexpr uint32 MAX_TAXI_NODE = TaxiMaskSize * 32;

    bool IsNodeInMask(TaxiMask const& mask, uint32 nodeId)
    {
        uint8 field = uint8((nodeId - 1) / 32);
        uint32 submask = 1 << ((nodeId - 1) % 32);
        return (mask[field] & submask) != 0;
    }

    // The playerbots fork adds WorldSession::IsBot(); stock AzerothCore doesn't have it. Looking for
    // it at compile time lets the module build on both.
    template <typename Session, typename = void>
    struct HasIsBot : std::false_type { };

    template <typename Session>
    struct HasIsBot<Session, std::void_t<decltype(std::declval<Session&>().IsBot())>> : std::true_type { };

    template <typename Session>
    bool IsBotSession(Session* session)
    {
        if constexpr (HasIsBot<Session>::value)
            return session->IsBot();
        else
            return false;
    }

    // Bots are skipped both ways: they don't add flight paths to their account and don't get taught
    // any.
    bool IsRealPlayer(Player const* player)
    {
        WorldSession* session = player ? player->GetSession() : nullptr;
        return session && !IsBotSession(session);
    }

    // Whether this character's knowing the node should count for the account. A death knight is
    // created knowing every flight path in Kalimdor and the Eastern Kingdoms, so without this one
    // new death knight would unlock the whole old world for every alt.
    bool ShouldRecord(Player const* player, uint32 nodeId)
    {
        if (!IsNodeInMask(sTaxiNodesMask, nodeId))
            return false;

        if (player->getClass() == CLASS_DEATH_KNIGHT && !config.shareDeathKnightStartNodes
            && IsNodeInMask(sOldContinentsNodesMask, nodeId))
            return false;

        return true;
    }

    // Whether an account flight path may be taught to this character: only nodes its own faction
    // has a flight master at (neutral ones count for both), and the Ebon Hold and Shadow Vault
    // nodes only for death knights.
    bool CanShareWith(Player const* player, uint32 nodeId)
    {
        if (!IsNodeInMask(sTaxiNodesMask, nodeId))
            return false;

        TaxiMask const& factionMask = player->GetTeamId(true) == TEAM_HORDE ? sHordeTaxiNodesMask : sAllianceTaxiNodesMask;
        if (!IsNodeInMask(factionMask, nodeId))
            return false;

        if (player->getClass() != CLASS_DEATH_KNIGHT && IsNodeInMask(sDeathKnightTaxiNodesMask, nodeId))
            return false;

        return true;
    }

    void SaveNode(uint32 accountId, uint32 nodeId)
    {
        CharacterDatabase.Execute(
            "INSERT IGNORE INTO `accountwide_flight_paths` (`account_id`, `node`) VALUES ({}, {})",
            accountId, nodeId);
    }

    // Records every flight path this character knows, in one statement.
    void SaveNodes(Player const* player)
    {
        uint32 accountId = player->GetSession()->GetAccountId();
        std::string values;

        for (uint32 nodeId = 1; nodeId <= MAX_TAXI_NODE; ++nodeId)
        {
            if (!player->m_taxi.IsTaximaskNodeKnown(nodeId) || !ShouldRecord(player, nodeId))
                continue;

            if (!values.empty())
                values += ',';

            values += "(" + std::to_string(accountId) + "," + std::to_string(nodeId) + ")";
        }

        if (values.empty())
            return;

        CharacterDatabase.Execute(
            "INSERT IGNORE INTO `accountwide_flight_paths` (`account_id`, `node`) VALUES " + values);
    }

    // Sets the known bits directly. The client gets the whole mask every time a flight master's
    // map opens, so nothing has to be sent now, and the mask goes to `characters`.`taximask` with
    // the next save.
    void TeachNodes(Player* player)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT `node` FROM `accountwide_flight_paths` WHERE `account_id` = {}",
            player->GetSession()->GetAccountId());
        if (!result)
            return;

        uint32 learned = 0;
        do
        {
            uint32 nodeId = result->Fetch()[0].Get<uint32>();
            if (nodeId == 0 || nodeId > MAX_TAXI_NODE || !CanShareWith(player, nodeId))
                continue;

            if (player->m_taxi.SetTaximaskNode(nodeId))
                ++learned;
        }
        while (result->NextRow());

        if (learned && config.announce)
            ChatHandler(player->GetSession()).PSendSysMessage(
                "You learned {} flight path{} discovered by your other characters.", learned, learned == 1 ? "" : "s");
    }
}

class AccountWideFlightPathsWorldScript : public WorldScript
{
public:
    AccountWideFlightPathsWorldScript() : WorldScript("AccountWideFlightPathsWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled = sConfigMgr->GetOption<bool>("AccountWideFlightPaths.Enable", true);
        config.announce = sConfigMgr->GetOption<bool>("AccountWideFlightPaths.Announce", true);
        config.shareDeathKnightStartNodes = sConfigMgr->GetOption<bool>("AccountWideFlightPaths.ShareDeathKnightStartNodes", false);
    }
};

class AccountWideFlightPathsPlayerScript : public PlayerScript
{
public:
    AccountWideFlightPathsPlayerScript() : PlayerScript("AccountWideFlightPathsPlayerScript") { }

    // Save first, so a character that already knows flight paths adds them to the account right
    // away instead of on its first logout.
    void OnPlayerLogin(Player* player) override
    {
        if (!config.enabled || !IsRealPlayer(player))
            return;

        SaveNodes(player);
        TeachNodes(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!config.enabled || !IsRealPlayer(player))
            return;

        SaveNodes(player);
    }

    // Records a new flight path the moment it's discovered, so a crash before logout doesn't lose it.
    void OnPlayerLearnTaxiNode(Player const* player, uint32 nodeId) override
    {
        if (!config.enabled || !IsRealPlayer(player) || !ShouldRecord(player, nodeId))
            return;

        SaveNode(player->GetSession()->GetAccountId(), nodeId);
    }

    // Deleting the last character on an account forgets the account's flight paths too.
    void OnPlayerDelete(ObjectGuid guid, uint32 accountId) override
    {
        if (!config.enabled)
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT 1 FROM `characters` WHERE `account` = {} AND `guid` <> {} AND `deleteDate` IS NULL LIMIT 1",
            accountId, guid.GetCounter());
        if (result)
            return;

        CharacterDatabase.Execute("DELETE FROM `accountwide_flight_paths` WHERE `account_id` = {}", accountId);
    }
};

void AddAccountWideFlightPathsScripts()
{
    new AccountWideFlightPathsWorldScript();
    new AccountWideFlightPathsPlayerScript();
}
