/*
 * main.cpp
 *
 *  Created on: Mar 19, 2017
 *      Author: nullifiedcat
 */

#include "ipcb.hpp"
#include "cathookipc.hpp"
#include <vector>
#include <string>
#include <thread>
#include <chrono>

auto peer = std::make_unique<cat_ipc::Peer<server_data_s, user_data_s>>("cathook_followbot_server", false, true, true);

#define ESC_PRINTF(s, ...) printf("\e[" s, __VA_ARGS__)
#define ESC_CUP(x, y) ESC_PRINTF("%d;%dH", (y), (x))
#define ESC_EL(n) ESC_PRINTF("%dK", (n))
#define ESC_SGR(n) ESC_PRINTF("%dm", (n))
#define ESC_ED(n) ESC_PRINTF("%dJ", (n));

#define TEXT_BOLD ESC_SGR(1)
#define TEXT_NORMAL ESC_SGR(0)

const std::string classes[] = {
    "Unknown", "Scout",
    "Sniper", "Soldier",
    "Demoman", "Medic",
    "Heavy", "Pyro",
    "Spy", "Engineer"
};

const std::string teams[] = {
    "UNK", "SPEC", "RED", "BLU"
};

bool good_class(int clazz)
{
    return clazz > 0 && clazz < 10;
}

bool good_team(int team)
{
    return team >= 0 && team < 4;
}

void print_status()
{
    ESC_CUP(1, 1);
    ESC_ED(2);
    fflush(stdout);
    ESC_CUP(2, 2);
    TEXT_BOLD;
    printf("cathook IPC server");
    ESC_CUP(3, 4);
    printf("connected: ");
    TEXT_NORMAL;
    printf("%d / %d", peer->memory->peer_count, cat_ipc::max_peers);
    TEXT_BOLD;
    ESC_CUP(3, 5);
    printf("command count: ");
    TEXT_NORMAL;
    printf("%lu", peer->memory->command_count);
    TEXT_BOLD;
    ESC_CUP(3, 6);
    simple_ipc::CatMemoryPool::PoolInfo info{};
    peer->pool->statistics(info);
    printf("memory: ");
    TEXT_NORMAL;
    printf("%.1f%% free\n", static_cast<float>(info.free) / static_cast<float>(info.free + info.alloc) * 100.f);
    TEXT_BOLD;
    /*printf("command memory pool stats: ");
    CatMemoryPool::pool_info_s info;
    peer().pool->statistics(info);
    ESC_CUP(4, 8); ESC_EL(2);  printf("total:     ");
    ESC_CUP(4, 9); ESC_EL(2);  printf("free:      ");
    ESC_CUP(4, 10); ESC_EL(2); printf("allocated: ");
    TEXT_NORMAL;
    ESC_CUP(16, 8);  printf("%lu (%u blocks)", info.free + info.alloc, info.blkcnt);
    ESC_CUP(16, 9); printf("%lu (%u blocks)", info.free, info.freeblk);
    ESC_CUP(16, 10); printf("%lu (%u blocks)", info.alloc, info.allocblk);
    */
    int ypos = 8;
    ESC_CUP(2, ypos);
    printf("%-2s %-5s %-9s %-21s %s\n", "ID", "PID", "SteamID", "Server IP", "Name");
    printf("    %-5s %-9s %-4s   %-5s   %-5s  %-9s %s",
        "State",
        "Class",
        "Team",
        "Score",
        "Total",
        "Health",
        "Heartbeat");
    ypos += 3;
    TEXT_NORMAL;
    // Zeroth peer is the server.
    for (unsigned int i = 0; i < cat_ipc::max_peers; i++)
    {
        if (!peer->memory->peer_data[i].free)
        {
            ESC_CUP(2, ypos);
            const auto& data = peer->memory->peer_user_data[i];
            printf("%-2u %-5d %-9u %-21s %s\n",
                i,
                peer->memory->peer_data[i].pid,
                data.friendid,
                data.ingame.server,
                data.name);
            if (data.connected && data.ingame.good)
            {
                printf("    %-5s %-9s %-4s   %-5d   %-5d   %-4d/%-4d %ld\n",
                    data.ingame.life_state ? "Dead" : "Alive",
                    good_class(data.ingame.role) ? classes[data.ingame.role].c_str() : classes[0].c_str(),
                    good_team(data.ingame.team) ? teams[data.ingame.team].c_str() : teams[0].c_str(),
                    data.ingame.score, data.accumulated.score, data.ingame.health, data.ingame.health_max,
                    time(nullptr) - data.heartbeat);
            }
            else
            {
                printf("    %-5s %-9s %-4s   %-5s   %-5d   %-9s %ld\n",
                    "N/A", "N/A", "N/A", "N/A", data.accumulated.score, "N/A", time(nullptr) - data.heartbeat);
            }
            ypos += 2;
        }
    }
    ESC_CUP(1, ypos + 1);
    fflush(stdout);
}

[[noreturn]] int main(int argc, char** argv)
{
    peer->Connect();
    peer->memory->global_data.magic_number = 0x0DEADCA7;
    bool silent = false;

    // Convert C-style command line arguments to a vector of std::strings
    std::vector<std::string> args(argv + 1, argv + argc);

    for (const auto& arg : args)
        if (arg == "-s")
            silent = true;

    auto last_time = std::chrono::high_resolution_clock::now();
    while (true)
    {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_time);

        if (elapsed_time.count() >= 10)
        {
            peer->SweepDead();
            peer->ProcessCommands();
            last_time = current_time;
        }

        if (!silent)
            print_status();

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
