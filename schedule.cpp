#include <SFML/Graphics.hpp>
#include <iostream>
#include <algorithm>
#include <climits>
#include <vector>
#include <queue>
#include <sstream>
#include <iomanip>
#include <fstream>

using namespace std;

struct Process {
    int pid, arrival_time, burst_time, priority;
    int start_time = 0, completion_time = 0;
    int remaining_time = 0;
    int turnaround_time = 0, waiting_time = 0;
    bool finished = false;
    bool started = false;
    sf::Color color;
    sf::Vector2f position;
    sf::Vector2f target_position;
    bool is_executing = false;
    int queue_index = -1;
    int last_execution_time = 0;
    int time_slice_remaining = 0; // For Round Robin
};

struct SimulationData {
    vector<Process> processes;
    vector<int> sequence;
    int time_quantum;
};

class MLQVisualizer {
private:
    sf::RenderWindow window;
    sf::Font font;
    vector<Process> processes;
    vector<Process> original_processes; // Store original data
    vector<vector<Process*>> queues;
    vector<string> algorithm_names;
    vector<int> sequence;
    int time_quantum;
    int current_time;
    int current_executing_queue;
    Process* current_executing_process;
    bool simulation_running;
    bool simulation_paused;
    bool simulation_completed;
    
    // Animation variables
    float animation_speed;
    sf::Clock animation_clock;
    sf::Clock simulation_clock;
    
    // UI elements
    sf::Text title_text;
    sf::Text time_text;
    sf::Text status_text;
    sf::Text instructions_text;
    sf::Text averages_text;
    
    // Colors for different processes
    vector<sf::Color> process_colors = {
        sf::Color::Red, sf::Color::Green, sf::Color::Blue, sf::Color::Yellow,
        sf::Color::Magenta, sf::Color::Cyan, sf::Color(255, 165, 0), // Orange
        sf::Color(128, 0, 128), // Purple
        sf::Color(255, 192, 203), // Pink
        sf::Color(0, 128, 0) // Dark Green
    };

public:
    MLQVisualizer() : window(sf::VideoMode(1400, 900), "Multilevel Queue Scheduler Visualization"),
                      queues(4), animation_speed(1.0f), current_time(0), current_executing_queue(-1),
                      current_executing_process(nullptr), simulation_running(false), 
                      simulation_paused(false), simulation_completed(false) {
        
        // Try multiple font paths
        vector<string> font_paths = {
            "arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",           // Linux
            "/usr/share/fonts/TTF/DejaVuSans.ttf",                      // Arch Linux
            "/System/Library/Fonts/Arial.ttf",                          // macOS
            "C:/Windows/Fonts/arial.ttf",                               // Windows
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf" // Linux alternative
        };
        
        bool font_loaded = false;
        for (const string& path : font_paths) {
            if (font.loadFromFile(path)) {
                font_loaded = true;
                cout << "Loaded font: " << path << endl;
                break;
            }
        }
        
        if (!font_loaded) {
            cout << "Warning: Could not load any font. Using default font.\n";
        }
        
        algorithm_names = {"FCFS", "Priority", "SJF", "Round Robin"};
        
        setupUI();
        window.setFramerateLimit(60);
    }
    
    void setupUI() {
        // Only set font if it was loaded successfully
        if (font.getInfo().family != "") {
            title_text.setFont(font);
            time_text.setFont(font);
            status_text.setFont(font);
            instructions_text.setFont(font);
            averages_text.setFont(font);
        }
        
        title_text.setString("Multilevel Queue Scheduler Visualization");
        title_text.setCharacterSize(24);
        title_text.setFillColor(sf::Color::White);
        title_text.setPosition(10, 10);
        
        time_text.setCharacterSize(18);
        time_text.setFillColor(sf::Color::White);
        time_text.setPosition(10, 50);
        
        status_text.setCharacterSize(16);
        status_text.setFillColor(sf::Color::Yellow);
        status_text.setPosition(10, 80);
        
        instructions_text.setString("Controls: SPACE - Start/Pause, R - Reset, +/- - Speed, S - Save, L - Load, ESC - Exit");
        instructions_text.setCharacterSize(14);
        instructions_text.setFillColor(sf::Color::White);
        instructions_text.setPosition(10, 850);
        
        averages_text.setCharacterSize(16);
        averages_text.setFillColor(sf::Color::Green);
        averages_text.setPosition(800, 50);
    }
    
    void saveToFile(const string& filename) {
        ofstream file(filename);
        if (!file.is_open()) {
            cout << "Error: Could not open file for writing: " << filename << endl;
            return;
        }
        
        file << original_processes.size() << "\n";
        file << time_quantum << "\n";
        
        for (const auto& process : original_processes) {
            file << process.arrival_time << " " << process.burst_time << " " << process.priority << "\n";
        }
        
        for (int i = 0; i < 4; i++) {
            file << sequence[i];
            if (i < 3) file << " ";
        }
        file << "\n";
        
        file.close();
        cout << "Data saved to " << filename << endl;
    }
    
    bool loadFromFile(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cout << "Error: Could not open file for reading: " << filename << endl;
            return false;
        }
        
        int n;
        file >> n;
        file >> time_quantum;
        
        vector<Process> loaded_processes(n);
        for (int i = 0; i < n; i++) {
            loaded_processes[i].pid = i + 1;
            file >> loaded_processes[i].arrival_time >> loaded_processes[i].burst_time >> loaded_processes[i].priority;
        }
        
        vector<int> loaded_sequence(4);
        for (int i = 0; i < 4; i++) {
            file >> loaded_sequence[i];
        }
        
        file.close();
        
        initializeProcesses(loaded_processes, loaded_sequence, time_quantum);
        cout << "Data loaded from " << filename << endl;
        return true;
    }
    
    void initializeProcesses(const vector<Process>& input_processes, const vector<int>& sched_sequence, int quantum) {
        original_processes = input_processes;
        processes = input_processes;
        sequence = sched_sequence;
        time_quantum = quantum;
        
        // Assign colors to processes
        for (size_t i = 0; i < processes.size(); i++) {
            processes[i].color = process_colors[i % process_colors.size()];
            processes[i].remaining_time = processes[i].burst_time;
            processes[i].finished = false;
            processes[i].started = false;
            processes[i].time_slice_remaining = 0;
        }
        
        // Distribute processes to queues (round-robin distribution)
        for (int i = 0; i < 4; i++) {
            queues[i].clear();
        }
        
        for (size_t i = 0; i < processes.size(); i++) {
            int queue_index = i % 4;
            processes[i].queue_index = queue_index;
            queues[queue_index].push_back(&processes[i]);
        }
        
        positionProcesses();
        resetSimulation();
    }
    
    void positionProcesses() {
        float queue_y_positions[] = {150, 250, 350, 450};
        float process_size = 40;
        
        for (int q = 0; q < 4; q++) {
            for (size_t p = 0; p < queues[q].size(); p++) {
                queues[q][p]->position.x = 100 + p * (process_size + 15); // Increased spacing
                queues[q][p]->position.y = queue_y_positions[q];
                queues[q][p]->target_position = queues[q][p]->position;
            }
        }
    }
    
    void resetSimulation() {
        current_time = 0;
        current_executing_queue = -1;
        current_executing_process = nullptr;
        simulation_running = false;
        simulation_paused = false;
        simulation_completed = false;
        
        for (auto& process : processes) {
            process.remaining_time = process.burst_time;
            process.finished = false;
            process.started = false;
            process.is_executing = false;
            process.start_time = 0;
            process.completion_time = 0;
            process.turnaround_time = 0;
            process.waiting_time = 0;
            process.last_execution_time = 0;
            process.time_slice_remaining = 0;
        }
        
        positionProcesses();
    }
    
    void updateSimulation() {
        if (!simulation_running || simulation_paused || simulation_completed) return;
        
        if (simulation_clock.getElapsedTime().asMilliseconds() > (1000 / animation_speed)) {
            simulation_clock.restart();
            
            // Check if all processes are finished
            bool all_finished = true;
            for (const auto& process : processes) {
                if (!process.finished) {
                    all_finished = false;
                    break;
                }
            }
            
            if (all_finished) {
                simulation_completed = true;
                calculateFinalStatistics();
                return;
            }
            
            // Execute scheduling
            executeScheduling();
            current_time++;
        }
    }
    
    void executeScheduling() {
        // Reset execution states
        for (auto& process : processes) {
            process.is_executing = false;
            if (!process.finished) {
                // Better positioning to prevent overlap
                auto it = find(queues[process.queue_index].begin(), queues[process.queue_index].end(), &process);
                int position_in_queue = it - queues[process.queue_index].begin();
                process.target_position.x = 100 + position_in_queue * 55; // Increased spacing
                process.target_position.y = 150 + process.queue_index * 100; // Queue positioning
            }
        }
        
        // Check if current process should continue (for Round Robin or non-preemptive)
        if (current_executing_process && !current_executing_process->finished) {
            int current_algorithm = sequence[current_executing_queue];
            
            // For Round Robin, check if time slice is exhausted
            if (current_algorithm == 3) { // Round Robin
                if (current_executing_process->time_slice_remaining > 0) {
                    executeProcess(current_executing_process, current_algorithm);
                    return;
                }
            } else {
                // For non-preemptive algorithms, continue until process finishes
                executeProcess(current_executing_process, current_algorithm);
                return;
            }
        }
        
        // Find next process to execute (multilevel queue priority)
        current_executing_process = nullptr;
        current_executing_queue = -1;
        
        // Check queues in order of priority (Queue 0 has highest priority)
        for (int q = 0; q < 4; q++) {
            Process* selected = selectFromQueue(q);
            if (selected) {
                current_executing_process = selected;
                current_executing_queue = q;
                
                // Initialize time slice for Round Robin
                if (sequence[q] == 3) { // Round Robin
                    current_executing_process->time_slice_remaining = time_quantum;
                }
                
                executeProcess(current_executing_process, sequence[q]);
                break;
            }
        }
    }
    
    Process* selectFromQueue(int queue_index) {
        vector<Process*>& current_queue_processes = queues[queue_index];
        int algorithm = sequence[queue_index];
        
        // Find ready processes
        vector<Process*> ready_processes;
        for (auto* process : current_queue_processes) {
            if (process->arrival_time <= current_time && !process->finished) {
                ready_processes.push_back(process);
            }
        }
        
        if (ready_processes.empty()) {
            return nullptr;
        }
        
        return selectProcess(ready_processes, algorithm);
    }
    
    Process* selectProcess(vector<Process*>& ready_processes, int algorithm) {
        switch (algorithm) {
            case 0: // FCFS
                return *min_element(ready_processes.begin(), ready_processes.end(),
                    [](Process* a, Process* b) { 
                        if (a->arrival_time == b->arrival_time) {
                            return a->pid < b->pid;
                        }
                        return a->arrival_time < b->arrival_time; 
                    });
                
            case 1: // Priority Scheduling
                return *min_element(ready_processes.begin(), ready_processes.end(),
                    [](Process* a, Process* b) { 
                        if (a->priority == b->priority) {
                            return a->arrival_time < b->arrival_time;
                        }
                        return a->priority < b->priority; 
                    });
                
            case 2: // SJF
                return *min_element(ready_processes.begin(), ready_processes.end(),
                    [](Process* a, Process* b) { 
                        if (a->remaining_time == b->remaining_time) {
                            return a->arrival_time < b->arrival_time;
                        }
                        return a->remaining_time < b->remaining_time; 
                    });
                
            case 3: // Round Robin
                // Find the process that was executed least recently
                return *min_element(ready_processes.begin(), ready_processes.end(),
                    [](Process* a, Process* b) { 
                        if (a->last_execution_time == b->last_execution_time) {
                            return a->arrival_time < b->arrival_time;
                        }
                        return a->last_execution_time < b->last_execution_time; 
                    });
        }
        return nullptr;
    }
    
    void executeProcess(Process* process, int algorithm) {
        if (!process->started) {
            process->started = true;
            process->start_time = current_time;
        }
        
        process->is_executing = true;
        process->target_position.x = 680 + (current_time % 4) * 60; // Stagger positions to prevent overlap
        process->target_position.y = 200 + (current_time % 6) * 50;  // Vertical staggering too
        process->last_execution_time = current_time;
        
        // Execute for 1 time unit
        process->remaining_time -= 1;
        
        // For Round Robin, decrease time slice
        if (algorithm == 3 && process->time_slice_remaining > 0) {
            process->time_slice_remaining--;
        }
        
        if (process->remaining_time <= 0) {
            process->finished = true;
            process->completion_time = current_time + 1;
            process->turnaround_time = process->completion_time - process->arrival_time;
            process->waiting_time = process->turnaround_time - process->burst_time;
            
            // Clear current executing process
            current_executing_process = nullptr;
            current_executing_queue = -1;
        } else if (algorithm == 3 && process->time_slice_remaining <= 0) {
            // Time slice exhausted for Round Robin
            current_executing_process = nullptr;
            current_executing_queue = -1;
        }
    }
    
    void calculateFinalStatistics() {
        float total_turnaround_time = 0;
        float total_waiting_time = 0;
        
        for (const auto& process : processes) {
            total_turnaround_time += process.turnaround_time;
            total_waiting_time += process.waiting_time;
        }
        
        float avg_turnaround = total_turnaround_time / processes.size();
        float avg_waiting = total_waiting_time / processes.size();
        
        stringstream avg_stream;
        avg_stream << "COMPLETED!\n";
        avg_stream << "Avg TAT: " << fixed << setprecision(2) << avg_turnaround << "\n";
        avg_stream << "Avg WT: " << fixed << setprecision(2) << avg_waiting;
        averages_text.setString(avg_stream.str());
        
        cout << "\n=== SIMULATION COMPLETED ===\n";
        cout << "PID\tAT\tBT\tPrio\tCT\tTAT\tWT\n";
        for (const auto& process : processes) {
            cout << process.pid << "\t" << process.arrival_time << "\t" << process.burst_time
                 << "\t" << process.priority << "\t" << process.completion_time
                 << "\t" << process.turnaround_time << "\t" << process.waiting_time << "\n";
        }
        cout << "\nAverage Turnaround Time: " << avg_turnaround << "\n";
        cout << "Average Waiting Time: " << avg_waiting << "\n";
    }
    
    void updateAnimations() {
        float dt = animation_clock.restart().asSeconds();
        
        for (auto& process : processes) {
            // Smooth movement animation
            sf::Vector2f diff = process.target_position - process.position;
            if (abs(diff.x) > 1 || abs(diff.y) > 1) {
                process.position += diff * 5.0f * dt;
            } else {
                process.position = process.target_position;
            }
        }
    }
    
    void handleEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            
            if (event.type == sf::Event::KeyPressed) {
                switch (event.key.code) {
                    case sf::Keyboard::Space:
                        if (!simulation_running && !simulation_completed) {
                            simulation_running = true;
                            simulation_paused = false;
                        } else if (!simulation_completed) {
                            simulation_paused = !simulation_paused;
                        }
                        break;
                        
                    case sf::Keyboard::R:
                        resetSimulation();
                        break;
                        
                    case sf::Keyboard::Equal: // + key
                        animation_speed = min(5.0f, animation_speed + 0.5f);
                        break;
                        
                    case sf::Keyboard::Hyphen: // - key
                        animation_speed = max(0.5f, animation_speed - 0.5f);
                        break;
                        
                    case sf::Keyboard::S:
                        saveToFile("data.txt");
                        break;
                        
                    case sf::Keyboard::L:
                        loadFromFile("data.txt");
                        break;
                        
                    case sf::Keyboard::Escape:
                        window.close();
                        break;
                }
            }
        }
    }
    
    void render() {
        window.clear(sf::Color::Black);
        
        // Draw UI
        window.draw(title_text);
        
        stringstream time_stream;
        time_stream << "Time: " << current_time;
        time_text.setString(time_stream.str());
        window.draw(time_text);
        
        stringstream status_stream;
        if (simulation_completed) {
            status_stream << "Status: COMPLETED";
        } else if (simulation_running) {
            if (simulation_paused) {
                status_stream << "Status: PAUSED";
            } else {
                status_stream << "Status: RUNNING";
                if (current_executing_process) {
                    status_stream << " (Queue " << (current_executing_queue + 1) 
                                 << ": " << algorithm_names[sequence[current_executing_queue]] << ")";
                }
            }
        } else {
            status_stream << "Status: READY";
        }
        status_text.setString(status_stream.str());
        window.draw(status_text);
        
        window.draw(instructions_text);
        window.draw(averages_text);
        
        // Draw queue labels and backgrounds
        sf::RectangleShape queue_bg(sf::Vector2f(500, 80));
        sf::Text queue_label;
        if (font.getInfo().family != "") {
            queue_label.setFont(font);
        }
        queue_label.setCharacterSize(16);
        queue_label.setFillColor(sf::Color::White);
        
        float queue_y_positions[] = {140, 240, 340, 440};
        
        for (int i = 0; i < 4; i++) {
            queue_bg.setPosition(50, queue_y_positions[i]);
            
            // Highlight currently executing queue
            if (i == current_executing_queue) {
                queue_bg.setFillColor(sf::Color(100, 100, 0, 100));
            } else {
                queue_bg.setFillColor(sf::Color(50, 50, 50, 100));
            }
            
            queue_bg.setOutlineThickness(1);
            queue_bg.setOutlineColor(sf::Color::White);
            window.draw(queue_bg);
            
            stringstream label_stream;
            label_stream << "Queue " << i + 1 << " (Priority " << (i + 1) << "): " 
                        << algorithm_names[sequence[i]];
            if (sequence[i] == 3) { // Round Robin
                label_stream << " (TQ=" << time_quantum << ")";
            }
            queue_label.setString(label_stream.str());
            queue_label.setPosition(55, queue_y_positions[i] + 5);
            window.draw(queue_label);
        }
        
        // Draw execution area
        sf::RectangleShape exec_area(sf::Vector2f(300, 400));
        exec_area.setPosition(580, 150);
        exec_area.setFillColor(sf::Color(100, 0, 0, 50));
        exec_area.setOutlineThickness(2);
        exec_area.setOutlineColor(sf::Color::Red);
        window.draw(exec_area);
        
        sf::Text exec_label;
        if (font.getInfo().family != "") {
            exec_label.setFont(font);
        }
        exec_label.setString("Execution Area");
        exec_label.setCharacterSize(16);
        exec_label.setFillColor(sf::Color::White);
        exec_label.setPosition(650, 130);
        window.draw(exec_label);
        
        // Draw processes
        sf::CircleShape process_shape(20);
        sf::Text process_text;
        if (font.getInfo().family != "") {
            process_text.setFont(font);
        }
        process_text.setCharacterSize(12);
        process_text.setFillColor(sf::Color::Black);
        
        for (const auto& process : processes) {
            process_shape.setPosition(process.position.x - 20, process.position.y - 20);
            process_shape.setFillColor(process.color);
            
            if (process.is_executing) {
                process_shape.setOutlineThickness(3);
                process_shape.setOutlineColor(sf::Color::White);
            } else if (process.finished) {
                process_shape.setOutlineThickness(2);
                process_shape.setOutlineColor(sf::Color::Green);
            } else {
                process_shape.setOutlineThickness(1);
                process_shape.setOutlineColor(sf::Color::White);
            }
            
            window.draw(process_shape);
            
            // Draw process ID
            stringstream process_stream;
            process_stream << "P" << process.pid;
            process_text.setString(process_stream.str());
            process_text.setPosition(process.position.x - 10, process.position.y - 8);
            window.draw(process_text);
            
            // Draw remaining time
            if (!process.finished) {
                sf::Text remaining_text;
                if (font.getInfo().family != "") {
                    remaining_text.setFont(font);
                }
                remaining_text.setCharacterSize(10);
                remaining_text.setFillColor(sf::Color::Yellow);
                stringstream remaining_stream;
                remaining_stream << process.remaining_time;
                if (process.is_executing && sequence[current_executing_queue] == 3) {
                    remaining_stream << "/" << process.time_slice_remaining;
                }
                remaining_text.setString(remaining_stream.str());
                remaining_text.setPosition(process.position.x - 5, process.position.y + 25);
                window.draw(remaining_text);
            }
        }
        
        // Draw detailed statistics
        sf::Text stats_text;
        if (font.getInfo().family != "") {
            stats_text.setFont(font);
        }
        stats_text.setCharacterSize(12);
        stats_text.setFillColor(sf::Color::Cyan);
        stats_text.setPosition(900, 150);
        
        stringstream stats_stream;
        stats_stream << "Process Details:\n\n";
        stats_stream << "PID AT BT P  CT TAT WT\n";
        for (const auto& process : processes) {
            stats_stream << "P" << process.pid << "  " << setw(2) << process.arrival_time 
                        << " " << setw(2) << process.burst_time
                        << " " << setw(1) << process.priority;
            if (process.finished) {
                stats_stream << " " << setw(3) << process.completion_time
                           << " " << setw(3) << process.turnaround_time 
                           << " " << setw(2) << process.waiting_time;
            } else {
                stats_stream << "  --  --  --";
            }
            stats_stream << "\n";
        }
        
        stats_text.setString(stats_stream.str());
        window.draw(stats_text);
        
        window.display();
    }
    
    void run() {
        while (window.isOpen()) {
            handleEvents();
            updateSimulation();
            updateAnimations();
            render();
        }
    }
};

int main() {
    cout << "=== Multilevel Queue Scheduler Visualizer ===\n";
    
    MLQVisualizer visualizer;
    visualizer.loadFromFile("data.txt");
    visualizer.run();

    return 0;
}
