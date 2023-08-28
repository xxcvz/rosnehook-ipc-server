/*
 * console.cpp
 *
 *  Created on: Aug 12, 2017
 *      Author: nullifiedcat
 */

#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <ctime>

#include <csignal>
#include <unistd.h>

#include "ipcb.hpp"
#include "cathookipc.hpp"

#include "json.hpp"

using json = nlohmann::json;
using peer_t = cat_ipc::Peer<server_data_s, user_data_s>;

std::unordered_map<std::string, std::function<json(const json&)>> commands{};
std::unique_ptr<peer_t> peer = nullptr;

bool has_key(const json& o, const std::string& key)
{
    return o.find(key) != o.end();
}

void ReplaceString(std::string& input, const std::string& what, const std::string& with_what)
{
    size_t index;
    index = input.find(what);
    while (index != std::string::npos)
    {
        input.replace(index, what.size(), with_what);
        index = input.find(what, index + with_what.size());
    }
}

json query_peer(unsigned int id)
{
    if (!peer || !peer->connected)
        throw std::runtime_error("not connected to ipc server");

    if (id > cat_ipc::max_peers - 1)
        throw std::out_of_range("peer out of range");

    json result{};

    const auto& sdata = peer->memory->peer_data[id];
    const auto& udata = peer->memory->peer_user_data[id];

    if (peer->IsPeerDead(static_cast<int>(id)))
    {
        result["dead"] = true;
        return result;
    }

    result["name"] = std::string(udata.name);
    result["friendid"] = udata.friendid;
    result["connected"] = udata.connected;
    result["heartbeat"] = udata.heartbeat;
    result["ts_injected"] = udata.ts_injected;
    result["ts_connected"] = udata.ts_connected;
    result["ts_disconnected"] = udata.ts_disconnected;

    result["accumulated"] = json{};
    result["accumulated"]["kills"] = udata.accumulated.kills;
    result["accumulated"]["deaths"] = udata.accumulated.deaths;
    result["accumulated"]["score"] = udata.accumulated.score;
    result["accumulated"]["shots"] = udata.accumulated.shots;
    result["accumulated"]["hits"] = udata.accumulated.hits;
    result["accumulated"]["headshots"] = udata.accumulated.headshots;

    result["ingame"] = json{};
    result["ingame"]["good"] = udata.ingame.good;
    result["ingame"]["kills"] = udata.ingame.kills;
    result["ingame"]["deaths"] = udata.ingame.deaths;
    result["ingame"]["score"] = udata.ingame.score;
    result["ingame"]["shots"] = udata.ingame.shots;
    result["ingame"]["hits"] = udata.ingame.hits;
    result["ingame"]["headshots"] = udata.ingame.headshots;
    result["ingame"]["team"] = udata.ingame.team;
    result["ingame"]["role"] = udata.ingame.role;
    result["ingame"]["life_state"] = udata.ingame.life_state;
    result["ingame"]["health"] = udata.ingame.health;
    result["ingame"]["health_max"] = udata.ingame.health_max;
    result["ingame"]["x"] = udata.ingame.x;
    result["ingame"]["y"] = udata.ingame.y;
    result["ingame"]["z"] = udata.ingame.z;
    result["ingame"]["player_count"] = udata.ingame.player_count;
    result["ingame"]["bot_count"] = udata.ingame.bot_count;
    result["ingame"]["server"] = std::string(udata.ingame.server);
    result["ingame"]["mapname"] = std::string(udata.ingame.mapname);

    result["pid"] = sdata.pid;
    result["starttime"] = sdata.starttime;

    return result;
}

namespace cmd
{
json exec(const json& args)
{
    if (!peer || !peer->connected)
        throw std::runtime_error("not connected to ipc server");
    if (!has_key(args, "target"))
        throw std::runtime_error("undefined pid");
    if (!has_key(args, "cmd"))
        throw std::runtime_error("undefined command");
    auto uid = args["target"].get<int>();
    if (uid > cat_ipc::max_peers)
        throw std::out_of_range("peer out of range");
    if (peer->IsPeerDead(uid))
        throw std::runtime_error("peer is not connected");

    std::string cmd = args["cmd"];
    ReplaceString(cmd, " && ", " ; ");
    if (cmd.length() >= 63)
        peer->SendMessage(nullptr, uid, ipc_commands::execute_client_cmd_long, cmd.c_str(), cmd.length() + 1);
    else
        peer->SendMessage(cmd.c_str(), uid, ipc_commands::execute_client_cmd, nullptr, 0);
    return json{};
}

json exec_all(const json& args)
{
    if (!peer || !peer->connected)
        throw std::runtime_error("not connected to ipc server");
    if (!has_key(args, "cmd"))
        throw std::runtime_error("undefined command");
    std::string cmd = args["cmd"];
    ReplaceString(cmd, " && ", " ; ");
    if (cmd.length() >= 63)
        peer->SendMessage(nullptr, -1, ipc_commands::execute_client_cmd_long, cmd.c_str(), cmd.length() + 1);
    else
        peer->SendMessage(cmd.c_str(), -1, ipc_commands::execute_client_cmd, nullptr, 0);
    return json{};
}

json query(const json& args)
{
    if (!peer || !peer->connected)
        throw std::runtime_error("not connected to ipc server");

    json result{};
    auto skipEmpty = has_key(args, "skipEmpty") && args["skipEmpty"].get<bool>();

    if (has_key(args, "ids"))
    {
        for (const auto& i : args["ids"])
        {
            unsigned int uid = i;
            if (skipEmpty && peer->IsPeerDead(static_cast<int>(uid)))
                continue;
            result[i.get<std::string>()] = query_peer(uid);
        }
    }
    else
    {
        for (unsigned int i = 0; i < cat_ipc::max_peers; i++)
        {
            if (skipEmpty && peer->IsPeerDead(static_cast<int>(i)))
                continue;
            result[std::to_string(i)] = query_peer(i);
        }
    }
    return result;
}

// Server Query
json squery(const json& args)
{
    if (!peer || !peer->connected)
        throw std::runtime_error("not connected to ipc server");
    json result{};
    const auto& mem = peer->memory;
    result["count"] = mem->peer_count;
    result["command_count"] = mem->command_count;
    return result;
}

json kill(const json& args)
{
    if (getuid() != 0)
        throw std::runtime_error("kill can only be used as root");
    if (!peer || !peer->connected)
        throw std::runtime_error("not connected to ipc server");
    if (!has_key(args, "pid"))
        throw std::runtime_error("undefined pid");
    pid_t uid = args["pid"].get<pid_t>();
    if (uid < 0 || uid > cat_ipc::max_peers - 1)
        throw std::out_of_range("peer out of range");
    if (peer->IsPeerDead(uid))
        throw std::runtime_error("already dead");
    ::kill(pid_t(peer->memory->peer_data[uid].pid), SIGKILL);
    return json{};
}

json echo(const json& args)
{
    json result{};
    result["args"] = args;
    return result;
}

json connect(const json& args)
{
    if (peer)
        throw std::runtime_error("already connected");

    peer = std::make_unique<peer_t>(has_key(args, "server") ? args["server"].get<std::string>()
                                                            : "cathook_followbot_server", false, false, true);
    peer->Connect();

    return json{};
}

json disconnect(const json& args)
{
    if (!peer || !peer->connected)
        throw std::runtime_error("not connected to ipc server");
    peer.reset();
    return json{};
}
}

std::vector<std::string> split(const std::string& input)
{
    std::vector<std::string> result{};
    std::string current;
    bool quote = false,
        escape = false;

    for (const auto& c : input)
    {
        if (escape)
        {
            current.push_back(c);
            escape = false;
        }
        else if (c == '\\')
            escape = true;
        else if (c == '"')
            quote = !quote;
        else if (!quote && (c == ' ' || c == '\n' || c == '\t'))
        {
            if (!current.empty())
            {
                result.push_back(current);
                current.clear();
            }
        }
        else
            current.push_back(c);
    }

    if (!current.empty())
        result.push_back(current);

    return result;
}

int main()
{
    commands["exec"] = &cmd::exec;
    commands["exec_all"] = &cmd::exec_all;
    commands["query"] = &cmd::query;
    commands["kill"] = &cmd::kill;
    commands["echo"] = &cmd::echo;
    commands["connect"] = &cmd::connect;
    commands["disconnect"] = &cmd::disconnect;
    commands["squery"] = &cmd::squery;

    std::cout << json{{ "init", time(nullptr) }} << std::endl;

    while (true)
    {
        std::string input;
        std::string cmdid{ "undefined" };
        std::getline(std::cin, input);
        try
        {
            auto args = json::parse(input);
            if (!has_key(args, "command"))
                throw std::runtime_error("empty command");
            if (has_key(args, "cmdid"))
                cmdid = args["cmdid"];
            if (args["command"] == "exit" || args["command"] == "quit")
            {
                std::cout << json{{ "exit", time(nullptr) }} << std::endl;
                break;
            }
            if (commands.find(args["command"]) == commands.end())
                throw std::runtime_error("command not found");
            else
            {
                std::cout
                    << json{{ "status", "success" }, { "cmdid", cmdid }, { "result", commands[args["command"]](args) }}
                    << std::endl;
            }
        }
        catch (std::exception& ex)
        {
            std::cerr << json{{ "status", "error" }, { "cmdid", cmdid }, { "error", std::string(ex.what()) }}
                      << std::endl;
        }
    }
}
