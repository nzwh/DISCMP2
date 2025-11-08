#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <random>
#include <chrono>
#include <atomic>

using namespace std;

struct Config {
    int max_instances;
    int min_clear_time;
    int max_clear_time;
} config;

struct PlayerQueues {
    queue<int> tanks;
    queue<int> healers;
    queue<int> dps;
    
    bool can_form_full_party() const {
        return !tanks.empty() && !healers.empty() && dps.size() >= 3;
    }
    
    size_t remaining_players() const {
        return tanks.size() + healers.size() + dps.size();
    }
} player_queues;

struct PartyComposition {
    int id;
    int tank_id, healer_id;
    int dps_ids[3];
    
    void display() const {
        cout << "Tank: " << tank_id << ", Healer: " << healer_id 
             << ", DPS: " << dps_ids[0] << ", " << dps_ids[1] << ", " << dps_ids[2];
    }
};

struct InstanceState {
    int id;
    bool occupied;
    PartyComposition current_party;
    int completed_runs;
    int cumulative_time;
    
    InstanceState() : id(0), occupied(false), completed_runs(0), cumulative_time(0) {}
};

vector<InstanceState> dungeon_instances;
atomic<int> running_instances(0);
atomic<int> next_party_num(1);
atomic<int> next_player_num(1);
atomic<bool> shutdown_flag(false);

mutex global_mutex;
condition_variable matchmaker_cv;

random_device rng_device;
mt19937 random_generator(rng_device());

void show_instance_states() {
    lock_guard<mutex> guard(global_mutex);
    cout << "\n┌─────────────────────────────────┐" << endl;
    cout << "│   Dungeon Instance Status       │" << endl;
    cout << "└─────────────────────────────────┘" << endl;
    
    for (const auto& instance : dungeon_instances) {
        cout << "  Instance " << instance.id << ": ";
        if (instance.occupied) {
            cout << "ACTIVE (Party #" << instance.current_party.id << ")" << endl;
        } else {
            cout << "EMPTY" << endl;
        }
    }
    cout << endl;
}

void show_final_report() {
    cout << "\n╔═════════════════════════════════════════╗" << endl;
    cout << "║        FINAL STATISTICS REPORT          ║" << endl;
    cout << "╚═════════════════════════════════════════╝" << endl;
    
    int total_runs = 0;
    int total_duration = 0;
    
    for (const auto& instance : dungeon_instances) {
        cout << "  Instance " << instance.id << " │ " 
             << instance.completed_runs << " parties │ "
             << instance.cumulative_time << "s total" << endl;
        total_runs += instance.completed_runs;
        total_duration += instance.cumulative_time;
    }
    
    cout << "\n  ─────────────────────────────────────" << endl;
    cout << "  Total Parties: " << total_runs << endl;
    cout << "  Combined Time: " << total_duration << "s" << endl;
    cout << "  ─────────────────────────────────────" << endl;
}

void show_queue_status() {
    lock_guard<mutex> guard(global_mutex);
    cout << "\n┌─── Queue Status ───┐" << endl;
    cout << "│ Tanks:   " << player_queues.tanks.size() << endl;
    cout << "│ Healers: " << player_queues.healers.size() << endl;
    cout << "│ DPS:     " << player_queues.dps.size() << endl;
    cout << "└────────────────────┘" << endl;
}

PartyComposition assemble_party() {
    PartyComposition party;
    party.id = next_party_num++;
    
    party.tank_id = player_queues.tanks.front();
    player_queues.tanks.pop();
    
    party.healer_id = player_queues.healers.front();
    player_queues.healers.pop();
    
    for (int i = 0; i < 3; i++) {
        party.dps_ids[i] = player_queues.dps.front();
        player_queues.dps.pop();
    }
    
    return party;
}

int find_free_instance() {
    for (size_t i = 0; i < dungeon_instances.size(); i++) {
        if (!dungeon_instances[i].occupied) {
            return i;
        }
    }
    return -1;
}

void execute_dungeon_run(int instance_index, PartyComposition party) {
    uniform_int_distribution<> time_dist(config.min_clear_time, config.max_clear_time);
    int run_duration = time_dist(random_generator);
    
    {
        lock_guard<mutex> guard(global_mutex);
        cout << "\n[→] Party " << party.id << " → Instance " 
             << dungeon_instances[instance_index].id << endl;
        cout << "    ";
        party.display();
        cout << " | Duration: " << run_duration << "s" << endl;
    }
    
    this_thread::sleep_for(chrono::seconds(run_duration));
    
    {
        lock_guard<mutex> guard(global_mutex);
        dungeon_instances[instance_index].occupied = false;
        dungeon_instances[instance_index].completed_runs++;
        dungeon_instances[instance_index].cumulative_time += run_duration;
        running_instances--;
        
        cout << "[✓] Party " << party.id << " cleared Instance " 
             << dungeon_instances[instance_index].id << " (" << run_duration << "s)" << endl;
    }
    
    matchmaker_cv.notify_all();
}

void run_matchmaker() {
    while (true) {
        unique_lock<mutex> lock(global_mutex);
        
        matchmaker_cv.wait(lock, []() {
            return (player_queues.can_form_full_party() && running_instances < config.max_instances) 
                   || (!player_queues.can_form_full_party() && shutdown_flag.load());
        });
        
        if (!player_queues.can_form_full_party() && shutdown_flag.load()) {
            break;
        }
        
        if (player_queues.can_form_full_party() && running_instances < config.max_instances) {
            PartyComposition new_party = assemble_party();
            int free_slot = find_free_instance();
            
            if (free_slot != -1) {
                dungeon_instances[free_slot].occupied = true;
                dungeon_instances[free_slot].current_party = new_party;
                running_instances++;
                
                thread(execute_dungeon_run, free_slot, new_party).detach();
            }
        }
    }
}

void gather_user_input() {
    int num_tanks, num_healers, num_dps;
    
    cout << "=== Dungeon Matchmaker Configuration ===" << endl;
    cout << "Maximum concurrent instances: ";
    cin >> config.max_instances;
    
    cout << "Number of tank players: ";
    cin >> num_tanks;
    
    cout << "Number of healer players: ";
    cin >> num_healers;
    
    cout << "Number of DPS players: ";
    cin >> num_dps;
    
    cout << "Minimum clear time (seconds): ";
    cin >> config.min_clear_time;
    
    cout << "Maximum clear time (seconds): ";
    cin >> config.max_clear_time;
    
    dungeon_instances.resize(config.max_instances);
    for (int i = 0; i < config.max_instances; i++) {
        dungeon_instances[i].id = i + 1;
    }
    
    for (int i = 0; i < num_tanks; i++) {
        player_queues.tanks.push(next_player_num++);
    }
    for (int i = 0; i < num_healers; i++) {
        player_queues.healers.push(next_player_num++);
    }
    for (int i = 0; i < num_dps; i++) {
        player_queues.dps.push(next_player_num++);
    }
    
    cout << "\n[Starting matchmaking process...]" << endl;
    show_queue_status();
}

int main() {
    gather_user_input();
    show_instance_states();
    
    thread matchmaker_thread(run_matchmaker);
    
    while (true) {
        bool should_continue = false;
        {
            lock_guard<mutex> guard(global_mutex);
            should_continue = (running_instances > 0 || player_queues.can_form_full_party());
        }
        
        if (!should_continue) break;
        
        this_thread::sleep_for(chrono::seconds(2));
        show_instance_states();
    }
    
    {
        lock_guard<mutex> guard(global_mutex);
        shutdown_flag.store(true);
    }
    matchmaker_cv.notify_all();
    
    matchmaker_thread.join();
    
    cout << "\n[Matchmaking process completed]" << endl;
    show_instance_states();
    
    {
        lock_guard<mutex> guard(global_mutex);
        if (player_queues.remaining_players() > 0) {
            cout << "\nPlayers still in queue:" << endl;
            cout << "  Tanks: " << player_queues.tanks.size() << endl;
            cout << "  Healers: " << player_queues.healers.size() << endl;
            cout << "  DPS: " << player_queues.dps.size() << endl;
        } else {
            cout << "\n[All players successfully matched!]" << endl;
        }
    }
    
    show_final_report();
    
    return 0;
}