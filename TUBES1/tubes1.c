/*
 * TUBES I - Simulasi Antrian Perjalanan Bus
 * Problem 2.38, Averill M. Law, Simulation Modeling and Analysis.
 *
 * Discrete-event simulation implemented in C with SIMLIB.
 * All simulation time units are hours.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../SIMLIB/simlib.h"

#define NUM_LOCATIONS             3
#define BUS_CAPACITY_DEFAULT      20
#define BUS_SPEED_DEFAULT         30.0
#define MIN_STOP_TIME             (5.0 / 60.0)
#define SECONDS_PER_HOUR          3600.0
#define MINUTES_PER_HOUR          60.0

/* Event types. */
#define EVENT_ARRIVAL_1           1
#define EVENT_ARRIVAL_2           2
#define EVENT_ARRIVAL_3           3
#define EVENT_BUS_ARRIVAL         4
#define EVENT_UNLOAD_COMPLETE     5
#define EVENT_LOAD_COMPLETE       6
#define EVENT_STOP_TIMER          7
#define EVENT_BUS_DEPARTURE       8
#define EVENT_END_SIMULATION      9

/* SIMLIB lists.  Lists 1-3 are location queues. */
#define LIST_QUEUE_1              1
#define LIST_QUEUE_2              2
#define LIST_QUEUE_3              3
#define LIST_BUS_TO_1             4
#define LIST_BUS_TO_2             5
#define LIST_BUS_TO_3             6

/* SIMLIB statistic variables. */
#define STAT_BUS_ONBOARD          1
#define STAT_QUEUE_DELAY_BASE     0       /* variables 1-3 */
#define STAT_SYSTEM_TIME_BASE     3       /* variables 4-6 */
#define STAT_STOP_TIME_BASE       6       /* variables 7-9 */
#define STAT_LOOP_TIME            10

#define PHASE_NONE                0
#define PHASE_UNLOADING           1
#define PHASE_LOADING             2
#define PHASE_WAITING             3
#define PHASE_DEPARTURE_SCHEDULED 4

#define EPS                       1.0e-9

/* Input and output. */
static FILE *infile;
static FILE *outfile;

/* Model parameters. */
static double length_simulation;
static double arrival_rate[NUM_LOCATIONS + 1];
static int bus_capacity;
static double bus_speed;

/* Bus state. */
static int bus_active;
static int current_stop;
static int bus_phase;
static int minimum_stop_elapsed;
static int onboard;
static int seats_reserved_for_loading;
static double stop_arrival_time;
static double minimum_departure_time;
static double last_car_rental_departure;
static long long total_arrivals;
static long long total_completed;
static long long checked_events;
static int state_checks_passed = 1;

static const char *location_name[NUM_LOCATIONS + 1] = {
    "", "Air terminal 1", "Air terminal 2", "Car rental"
};

/* Counterclockwise route: 3 -> 1 -> 2 -> 3. */
static const int next_stop[NUM_LOCATIONS + 1] = {0, 2, 3, 1};
static const double distance_to_next[NUM_LOCATIONS + 1] = {0.0, 1.0, 4.5, 4.5};

/* Destination probabilities for arrivals at the car rental. */
static double destination_distribution[26];

static int queue_list(int location)
{
    return location;
}

static int bus_list_for_destination(int destination)
{
    return 3 + destination;
}

static void schedule_arrival(int location);
static void schedule_bus_event(double time, int event_type, int location);
static void begin_unloading(void);
static void begin_loading_or_waiting(void);
static void start_loading(void);
static void maybe_schedule_departure(void);
static void handle_person_arrival(int location);
static void handle_bus_arrival(void);
static void handle_unload_complete(void);
static void handle_load_complete(void);
static void handle_stop_timer(void);
static void handle_bus_departure(void);
static void report(void);
static int validate_input(void);
static int verify_state(void);
static int report_verification(void);

/* Read one complete whitespace-delimited token; reject oversized tokens. */
static int read_input_token(char *token, size_t size)
{
    int character;
    size_t length = 0;

    do {
        character = fgetc(infile);
    } while (character != EOF && isspace((unsigned char) character));

    if (character == EOF) {
        return 0;
    }
    do {
        if (length + 1 >= size) {
            return 0;
        }
        token[length++] = (char) character;
        character = fgetc(infile);
    } while (character != EOF && !isspace((unsigned char) character));
    token[length] = '\0';
    return !ferror(infile);
}

static int read_input_double(double *value)
{
    char token[128];
    char *end;

    if (!read_input_token(token, sizeof token)) {
        return 0;
    }
    errno = 0;
    *value = strtod(token, &end);
    return errno == 0 && end != token && *end == '\0';
}

static int validate_input(void)
{
    int location;

    if (!isfinite(length_simulation) || length_simulation <= 0.0) {
        fprintf(stderr,
                "Invalid simulation duration: must be finite and greater than zero.\n");
        return 0;
    }

    for (location = 1; location <= NUM_LOCATIONS; ++location) {
        if (!isfinite(arrival_rate[location]) || arrival_rate[location] <= 0.0) {
            fprintf(stderr,
                    "Invalid arrival rate at location %d: must be finite and greater than zero.\n",
                    location);
            return 0;
        }
    }

    if (bus_capacity <= 0) {
        fprintf(stderr,
                "Invalid bus capacity: must be a positive integer.\n");
        return 0;
    }

    if (!isfinite(bus_speed) || bus_speed <= 0.0) {
        fprintf(stderr,
                "Invalid bus speed: must be finite and greater than zero.\n");
        return 0;
    }

    return 1;
}

static void schedule_arrival(int location)
{
    const int event_type = EVENT_ARRIVAL_1 + location - 1;
    const double mean_interarrival = 1.0 / arrival_rate[location];

    event_schedule(sim_time + expon(mean_interarrival, location), event_type);
}

static void schedule_bus_event(double time, int event_type, int location)
{
    transfer[3] = (double) location;
    event_schedule(time, event_type);
}

static void begin_unloading(void)
{
    const int list = bus_list_for_destination(current_stop);

    if (list_size[list] > 0) {
        bus_phase = PHASE_UNLOADING;
        transfer[3] = (double) current_stop;
        event_schedule(sim_time +
                       uniform(16.0 / SECONDS_PER_HOUR,
                               24.0 / SECONDS_PER_HOUR, 4),
                       EVENT_UNLOAD_COMPLETE);
    } else {
        begin_loading_or_waiting();
    }
}

static void begin_loading_or_waiting(void)
{
    bus_phase = PHASE_WAITING;
    if (list_size[queue_list(current_stop)] > 0 &&
        onboard + seats_reserved_for_loading < bus_capacity) {
        start_loading();
    } else {
        maybe_schedule_departure();
    }
}

static void start_loading(void)
{
    double person_arrival_time;
    int destination;

    if (!bus_active || bus_phase == PHASE_UNLOADING ||
        bus_phase == PHASE_LOADING ||
        list_size[queue_list(current_stop)] == 0 ||
        onboard + seats_reserved_for_loading >= bus_capacity) {
        return;
    }

    /* Remove the person when boarding starts: this is the queue-delay epoch. */
    list_remove(FIRST, queue_list(current_stop));
    person_arrival_time = transfer[1];
    destination = (int) transfer[3];
    sampst(sim_time - person_arrival_time,
           STAT_QUEUE_DELAY_BASE + current_stop);

    ++seats_reserved_for_loading;
    bus_phase = PHASE_LOADING;

    /* Attributes 3-5 are carried by the load-completion event. */
    transfer[3] = (double) destination;
    transfer[4] = person_arrival_time;
    transfer[5] = (double) current_stop;
    event_schedule(sim_time +
                   uniform(15.0 / SECONDS_PER_HOUR,
                           25.0 / SECONDS_PER_HOUR, 5),
                   EVENT_LOAD_COMPLETE);
}

static void maybe_schedule_departure(void)
{
    if (bus_active && bus_phase == PHASE_WAITING &&
        sim_time + EPS >= minimum_departure_time) {
        bus_phase = PHASE_DEPARTURE_SCHEDULED;
        schedule_bus_event(sim_time, EVENT_BUS_DEPARTURE, current_stop);
    }
}

static void handle_person_arrival(int location)
{
    int destination;

    /* Keep one independent exponential arrival process at each location. */
    schedule_arrival(location);

    transfer[1] = sim_time;          /* Arrival time in the system. */
    transfer[2] = (double) location;
    if (location == 3) {
        destination = random_integer(destination_distribution, 6);
    } else {
        destination = 3;
    }
    transfer[3] = (double) destination;
    list_file(LAST, queue_list(location));
    ++total_arrivals;

    /* A person arriving while the bus is waiting may start a new loading
       operation, provided the five-minute minimum stop has not expired. */
    if (bus_active && current_stop == location &&
        bus_phase == PHASE_WAITING && !minimum_stop_elapsed &&
        onboard + seats_reserved_for_loading < bus_capacity) {
        start_loading();
    }
}

static void handle_bus_arrival(void)
{
    current_stop = (int) transfer[3];
    bus_active = 1;
    bus_phase = PHASE_UNLOADING;
    minimum_stop_elapsed = 0;
    stop_arrival_time = sim_time;
    minimum_departure_time = sim_time + MIN_STOP_TIME;

    /* The timer lets arrivals during the mandatory five-minute dwell start
       loading if the bus is otherwise idle. */
    schedule_bus_event(minimum_departure_time, EVENT_STOP_TIMER, current_stop);
    begin_unloading();
}

static void handle_unload_complete(void)
{
    const int list = bus_list_for_destination(current_stop);
    int origin;

    list_remove(FIRST, list);
    origin = (int) transfer[2];
    sampst(sim_time - transfer[1], STAT_SYSTEM_TIME_BASE + origin);
    --onboard;
    ++total_completed;
    timest((double) onboard, STAT_BUS_ONBOARD);

    if (list_size[list] > 0) {
        transfer[3] = (double) current_stop;
        event_schedule(sim_time +
                       uniform(16.0 / SECONDS_PER_HOUR,
                               24.0 / SECONDS_PER_HOUR, 4),
                       EVENT_UNLOAD_COMPLETE);
    } else {
        begin_loading_or_waiting();
    }
}

static void handle_load_complete(void)
{
    const int destination = (int) transfer[3];
    const int origin = (int) transfer[5];

    transfer[1] = transfer[4];         /* Restore passenger attributes. */
    transfer[2] = (double) origin;
    transfer[3] = (double) destination;
    list_file(LAST, bus_list_for_destination(destination));

    --seats_reserved_for_loading;
    ++onboard;
    timest((double) onboard, STAT_BUS_ONBOARD);

    bus_phase = PHASE_WAITING;
    if (list_size[queue_list(current_stop)] > 0 &&
        onboard + seats_reserved_for_loading < bus_capacity) {
        start_loading();
    } else {
        maybe_schedule_departure();
    }
}

static void handle_stop_timer(void)
{
    minimum_stop_elapsed = 1;

    if (bus_phase == PHASE_UNLOADING || bus_phase == PHASE_LOADING) {
        /* The active operation is allowed to finish before departure. */
        return;
    }

    if (bus_phase == PHASE_WAITING) {
        if (list_size[queue_list(current_stop)] > 0 &&
            onboard + seats_reserved_for_loading < bus_capacity) {
            start_loading();
        } else {
            maybe_schedule_departure();
        }
    }
}

static void handle_bus_departure(void)
{
    const int departed_from = current_stop;
    const int next = next_stop[departed_from];
    const double stop_duration = sim_time - stop_arrival_time;

    sampst(stop_duration, STAT_STOP_TIME_BASE + departed_from);
    if (departed_from == 3) {
        sampst(sim_time - last_car_rental_departure, STAT_LOOP_TIME);
        last_car_rental_departure = sim_time;
    }

    bus_active = 0;
    bus_phase = PHASE_NONE;
    current_stop = 0;

    schedule_bus_event(sim_time +
                       distance_to_next[departed_from] / bus_speed,
                       EVENT_BUS_ARRIVAL, next);
}

static void print_sampst_row(int variable, const char *label)
{
    const double average = sampst(0.0, -variable);
    const double observations = transfer[2];
    const double maximum = observations == 0.0 ? 0.0 : transfer[3];
    const double minimum = observations == 0.0 ? 0.0 : transfer[4];

    fprintf(outfile, "%-18s %10.4f %12.0f %10.4f %10.4f\n",
            label, average * MINUTES_PER_HOUR, observations,
            maximum * MINUTES_PER_HOUR, minimum * MINUTES_PER_HOUR);
}

static void print_queue_row(int location)
{
    double average_number;
    double maximum_number;
    double average_delay;
    double delay_observations;
    double maximum_delay;
    double minimum_delay;

    filest(location);
    average_number = transfer[1];
    maximum_number = transfer[2] < 0.0 ? 0.0 : transfer[2];

    average_delay = sampst(0.0, -location);
    delay_observations = transfer[2];
    maximum_delay = delay_observations == 0.0 ? 0.0 : transfer[3];
    minimum_delay = delay_observations == 0.0 ? 0.0 : transfer[4];

    fprintf(outfile, "%-18s %10.4f %10.0f %12.4f %12.0f %10.4f %10.4f\n",
            location_name[location], average_number, maximum_number,
            average_delay * MINUTES_PER_HOUR, delay_observations,
            maximum_delay * MINUTES_PER_HOUR, minimum_delay * MINUTES_PER_HOUR);
}

static void report(void)
{
    double average_onboard;
    double maximum_onboard;
    double average_loop;
    double loop_observations;
    double maximum_loop;
    double minimum_loop;

    fprintf(outfile, "Car-rental bus simulation using SIMLIB\n");
    fprintf(outfile, "Problem 2.38 -- %g-hour run\n\n", length_simulation);
    fprintf(outfile, "Time unit: minute (internal simulation: hour). Distance unit: mile.\n");
    fprintf(outfile, "Locations and arrival rates:\n");
    fprintf(outfile, "  1 %-18s %8.3f people/hour\n", location_name[1], arrival_rate[1]);
    fprintf(outfile, "  2 %-18s %8.3f people/hour\n", location_name[2], arrival_rate[2]);
    fprintf(outfile, "  3 %-18s %8.3f people/hour\n", location_name[3], arrival_rate[3]);
    fprintf(outfile, "Bus capacity: %d people; speed: %.3f miles/hour\n", bus_capacity, bus_speed);
    fprintf(outfile, "Counterclockwise route: 3 -> 1 -> 2 -> 3\n");
    fprintf(outfile, "Travel distances: 3->1 = %.1f, 1->2 = %.1f, 2->3 = %.1f miles\n\n",
            distance_to_next[3], distance_to_next[1], distance_to_next[2]);

    fprintf(outfile, "(a)-(b) Queue length and queue-delay statistics\n");
    fprintf(outfile, "Location             Avg #      Max #   Avg delay (min) N delay  Max delay  Min delay\n");
    fprintf(outfile, "--------------------------------------------------------------------------------------\n");
    print_queue_row(1);
    print_queue_row(2);
    print_queue_row(3);
    fprintf(outfile, "\n");

    fprintf(outfile, "(c) Number of people on the bus\n");
    average_onboard = timest(0.0, -STAT_BUS_ONBOARD);
    maximum_onboard = transfer[2];
    fprintf(outfile, "Time-average number on bus: %.4f people\n", average_onboard);
    fprintf(outfile, "Maximum number on bus:      %.0f people\n\n", maximum_onboard);

    fprintf(outfile, "(d) Bus stop duration statistics (minutes)\n");
    fprintf(outfile, "Location             Average       N       Maximum       Minimum\n");
    fprintf(outfile, "------------------------------------------------------------------\n");
    print_sampst_row(STAT_STOP_TIME_BASE + 1, location_name[1]);
    print_sampst_row(STAT_STOP_TIME_BASE + 2, location_name[2]);
    print_sampst_row(STAT_STOP_TIME_BASE + 3, location_name[3]);
    fprintf(outfile, "\n");

    fprintf(outfile, "(e) Bus loop duration (departure from location 3 to next departure)\n");
    average_loop = sampst(0.0, -STAT_LOOP_TIME);
    loop_observations = transfer[2];
    maximum_loop = loop_observations == 0.0 ? 0.0 : transfer[3];
    minimum_loop = loop_observations == 0.0 ? 0.0 : transfer[4];
    fprintf(outfile, "Average: %.4f min; N: %.0f; Maximum: %.4f min; Minimum: %.4f min\n\n",
            average_loop * MINUTES_PER_HOUR, loop_observations,
            maximum_loop * MINUTES_PER_HOUR, minimum_loop * MINUTES_PER_HOUR);

    fprintf(outfile, "(f) Time in system by arrival location (completed trips only)\n");
    fprintf(outfile, "Arrival location    Average (min)     N       Maximum       Minimum\n");
    fprintf(outfile, "------------------------------------------------------------------\n");
    print_sampst_row(STAT_SYSTEM_TIME_BASE + 1, location_name[1]);
    print_sampst_row(STAT_SYSTEM_TIME_BASE + 2, location_name[2]);
    print_sampst_row(STAT_SYSTEM_TIME_BASE + 3, location_name[3]);
    fprintf(outfile, "\n");

    fprintf(outfile, "Simulation ended at %.4f min (%.4f h).\n",
            sim_time * MINUTES_PER_HOUR, sim_time);
    fprintf(outfile, "Queue delay is measured when boarding starts; time in system is\n");
    fprintf(outfile, "reported when a passenger is unloaded at the destination.\n");
}

/* Check after each handled event, including passengers currently boarding. */
static int verify_state(void)
{
    long long queued = 0, bus_records = 0;
    int location;

    for (location = 1; location <= NUM_LOCATIONS; ++location) {
        queued += list_size[queue_list(location)];
        bus_records += list_size[bus_list_for_destination(location)];
    }
    return onboard == bus_records && onboard >= 0 &&
           seats_reserved_for_loading >= 0 && seats_reserved_for_loading <= 1 &&
           (long long) onboard + seats_reserved_for_loading <= bus_capacity &&
           (bus_phase == PHASE_LOADING) == (seats_reserved_for_loading == 1) &&
           total_arrivals == total_completed + queued + bus_records +
                             seats_reserved_for_loading;
}

static int report_verification(void)
{
    long long queued = 0;
    double expected_loop = 3.0 * MIN_STOP_TIME;
    double minimum_loop, loop_samples, stop_samples = 0.0;
    double maximum_onboard;
    int location, capacity_ok, conservation_ok, loop_ok = 1, stop_ok = 1;
    int horizon_ok = sim_time == length_simulation;
    int passed;

    for (location = 1; location <= NUM_LOCATIONS; ++location) {
        queued += list_size[queue_list(location)];
        expected_loop += distance_to_next[location] / bus_speed;
        sampst(0.0, -(STAT_STOP_TIME_BASE + location));
        stop_samples += transfer[2];
        if (transfer[2] > 0.0 && transfer[4] + EPS < MIN_STOP_TIME) {
            stop_ok = 0;
        }
    }
    timest(0.0, -STAT_BUS_ONBOARD);
    maximum_onboard = transfer[2];
    capacity_ok = maximum_onboard <= bus_capacity;
    conservation_ok = total_arrivals == total_completed + queued + onboard +
                                       seats_reserved_for_loading;
    sampst(0.0, -STAT_LOOP_TIME);
    loop_samples = transfer[2];
    minimum_loop = loop_samples > 0.0 ? transfer[4] : 0.0;
    if (loop_samples > 0.0) {
        loop_ok = minimum_loop + EPS >= expected_loop;
    }
    fprintf(outfile, "\nInternal verification\n");
    fprintf(outfile, "[%s] Capacity: maximum %.0f; limit %d people\n",
            capacity_ok ? "PASS" : "FAIL", maximum_onboard, bus_capacity);
    fprintf(outfile, "[%s] Conservation: arrivals=%lld, completed=%lld,\n",
            conservation_ok ? "PASS" : "FAIL", total_arrivals, total_completed);
    fprintf(outfile, "       queued=%lld, onboard=%d, boarding=%d\n",
            queued, onboard, seats_reserved_for_loading);
    fprintf(outfile, "[%s] State invariants: %lld handled events checked\n",
            state_checks_passed ? "PASS" : "FAIL", checked_events);
    if (stop_samples > 0.0) {
        fprintf(outfile, "[%s] Minimum stop: completed stops >= %.4f min\n",
                stop_ok ? "PASS" : "FAIL", MIN_STOP_TIME * MINUTES_PER_HOUR);
    } else {
        fprintf(outfile, "[SKIP] Minimum stop: no completed stop\n");
    }
    if (loop_samples > 0.0) {
        fprintf(outfile, "[%s] Minimum loop: %.4f min; bound %.4f min\n",
                loop_ok ? "PASS" : "FAIL", minimum_loop * MINUTES_PER_HOUR,
                expected_loop * MINUTES_PER_HOUR);
    } else {
        fprintf(outfile, "[SKIP] Minimum loop: no completed loop\n");
    }
    fprintf(outfile, "[%s] Simulation horizon: %.4f min\n",
            horizon_ok ? "PASS" : "FAIL", sim_time * MINUTES_PER_HOUR);
    passed = capacity_ok && conservation_ok && state_checks_passed &&
             stop_ok && loop_ok && horizon_ok;
    fprintf(outfile, "%s (not statistical validation).\n",
            passed ? "All internal checks passed" : "Internal checks failed");
    return passed;
}

int main(int argc, char *argv[])
{
    int event_type;
    int verification_passed;
    char trailing_character;
    char capacity_token[128];
    char *capacity_end;
    long long capacity_input;
    const char *input_path = argc > 1 ? argv[1] : "tubes1.in";
    const char *output_path = argc > 2 ? argv[2] : "tubes1.out";
    struct stat input_status, output_status;

    if (argc > 3) {
        fprintf(stderr, "Usage: %s [input_file [output_file]]\n", argv[0]);
        return 1;
    }
    if (stat(input_path, &input_status) == 0 &&
        stat(output_path, &output_status) == 0 &&
        input_status.st_dev == output_status.st_dev &&
        input_status.st_ino == output_status.st_ino) {
        fprintf(stderr, "Input and output must refer to different files.\n");
        return 1;
    }
    infile = fopen(input_path, "r");
    if (infile == NULL) {
        fprintf(stderr, "Cannot open input: %s\n", input_path);
        return 1;
    }

    if (!read_input_double(&length_simulation) ||
        !read_input_double(&arrival_rate[1]) ||
        !read_input_double(&arrival_rate[2]) ||
        !read_input_double(&arrival_rate[3]) ||
        !read_input_token(capacity_token, sizeof capacity_token) ||
        !read_input_double(&bus_speed) ||
        fscanf(infile, " %c", &trailing_character) == 1 || ferror(infile)) {
        fprintf(stderr, "Invalid input format: %s\n", input_path);
        fclose(infile);
        return 1;
    }
    fclose(infile);

    errno = 0;
    capacity_input = strtoll(capacity_token, &capacity_end, 10);
    if (errno != 0 || capacity_end == capacity_token || *capacity_end != '\0' ||
        capacity_input < INT_MIN || capacity_input > INT_MAX) {
        fprintf(stderr, "Invalid bus capacity: must be an integer within int range.\n");
        return 1;
    }
    bus_capacity = (int) capacity_input;
    if (!validate_input()) {
        return 1;
    }

    outfile = fopen(output_path, "w");
    if (outfile == NULL) {
        fprintf(stderr, "Cannot open output: %s\n", output_path);
        return 1;
    }

    destination_distribution[1] = 0.583;
    destination_distribution[2] = 1.000;

    bus_active = 0;
    current_stop = 0;
    bus_phase = PHASE_NONE;
    minimum_stop_elapsed = 0;
    onboard = 0;
    seats_reserved_for_loading = 0;
    last_car_rental_departure = 0.0;

    /* maxatr must cover the passenger attributes carried by events. */
    maxatr = 5;
    init_simlib();
    timest(0.0, STAT_BUS_ONBOARD);

    /* Three independent arrival processes. */
    schedule_arrival(1);
    schedule_arrival(2);
    schedule_arrival(3);

    /* The bus begins at location 3 and leaves immediately counterclockwise. */
    schedule_bus_event(distance_to_next[3] / bus_speed,
                       EVENT_BUS_ARRIVAL, 1);
    event_schedule(length_simulation, EVENT_END_SIMULATION);

    while (1) {
        timing();
        event_type = next_event_type;

        if (event_type == EVENT_END_SIMULATION) {
            break;
        }

        switch (event_type) {
        case EVENT_ARRIVAL_1:
            handle_person_arrival(1);
            break;
        case EVENT_ARRIVAL_2:
            handle_person_arrival(2);
            break;
        case EVENT_ARRIVAL_3:
            handle_person_arrival(3);
            break;
        case EVENT_BUS_ARRIVAL:
            handle_bus_arrival();
            break;
        case EVENT_UNLOAD_COMPLETE:
            handle_unload_complete();
            break;
        case EVENT_LOAD_COMPLETE:
            handle_load_complete();
            break;
        case EVENT_STOP_TIMER:
            handle_stop_timer();
            break;
        case EVENT_BUS_DEPARTURE:
            handle_bus_departure();
            break;
        default:
            fprintf(stderr, "Unknown event type %d at time %.6f\n",
                    event_type, sim_time);
            fclose(outfile);
            return 1;
        }
        ++checked_events;
        state_checks_passed &= verify_state();
    }

    report();
    state_checks_passed &= verify_state();
    verification_passed = report_verification();
    if (fclose(outfile) != 0) {
        fprintf(stderr, "Cannot finish writing output: %s\n", output_path);
        return 1;
    }
    return verification_passed ? 0 : 1;
}
