﻿// TAPLite.cpp : This file contains the 'main' function. Program execution begins and ends here.

// This code is built based on the implementation available at:
// http://www.bgu.ac.il/~bargera/tntp/FW.zip
// tntp from Dr. Hillel Bar-Gera. It includes one of the most efficient Deque implementations of 
// the modified label correcting (MLC) algorithm in C. For more details, see mincostroutes.cpp. 
// The enhanced C++ counterpart has served as the path engine for Path4GMNS and TransOMS.
// https://github.com/jdlph/TAP101
#define M_PI 3.14159265358979323846
#define FLOAT_ACCURACY 1.0E-15
#define NO_COSTPARAMETERS 4
#define IVTT 0
#define OVTT 1
#define MONETARY 2
#define DIST 3
#define INVALID -1
#define MAX_MODE_TYPES  10

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define SQR(x) ((x) * (x))
#define FABS(x) ((x) >= 0 ? (x) : (-x))
#define VALID(x) ((x) != -1)

#include "TAPLite.h"


#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <cstdlib>

#include <cstdio>
#include <cmath>
#include <memory>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <omp.h>
#include <chrono>  // for high_resolution_clock
#include <deque>
#include <iomanip> // For std::setw and std::setfill
#include <unordered_map>  // For hash table (unordered_map)

using namespace std;

typedef double cost_vector[NO_COSTPARAMETERS];

#ifndef _WIN32
void fopen_s(FILE** file, const char* fileName, const char* mode)
{
   *file = fopen(fileName, mode);
}
#endif

struct link_record {
	int internal_from_node_id;
	int internal_to_node_id;
	int link_id;
	int link_type;
	int external_from_node_id;
	int external_to_node_id;

	double Lane_Capacity;
	double Lane_SaturationFlowRate; 
	double Link_Capacity;
	double lanes;
	double FreeTravelTime;
	double free_speed;
	double Cutoff_Speed;


	double VDF_Alpha;
	double BoverC;
	double VDF_Beta;
	double VDF_distance_factor; 
	double VDF_plf;
	double Q_fd, Q_n, Q_fp, Q_s;

	double length;
	double Speed;
	std::string allowed_uses;
	int mode_allowed_use[MAX_MODE_TYPES];
	double mode_MainVolume[MAX_MODE_TYPES];
	double mode_Base_demand_volume[MAX_MODE_TYPES];
	double mode_SubVolume[MAX_MODE_TYPES];
	double mode_SDVolume[MAX_MODE_TYPES];

	double mode_Toll[MAX_MODE_TYPES];
	double mode_AdditionalCost[MAX_MODE_TYPES];
	int non_uturn_flag; 
	double Travel_time;  // final travel time used in assignment 
	int vdf_type; 
	double BPR_TT;  // BPR_based travel time  for the entire assignment period  
	double QVDF_TT;  // QVDF_based travel time for the entire assignment period  

	bool b_withmovement_restrictions;
	double GenCost;
	double GenCostDer;
	double Ref_volume;
	double Base_demand_volume;
	double background_volume; 
	double Obs_volume[MAX_MODE_TYPES];
	double reassignment_volume;
	std::string geometry;

	int timing_arc_flag, cycle_length, start_green_time, end_green_time;

	link_record()
	{
		vdf_type = 0; 
		VDF_distance_factor = 0; 
		/*
		0	Classic BPR	BPR_Classic
			1	Conical(2 + √… – …) VDF	VDF_Conical
		*/
		link_type = 1;

		VDF_Alpha = 0.15;
		VDF_Beta = 4;

		VDF_plf = 1;

		Q_fd = 1.0;
		Q_n = 1.0;
		Q_fp = 0.5333 /* theta 15/8 */;
		Q_s = 1;

		Travel_time = 0;
		BPR_TT = 0;
		QVDF_TT = 0;
		Ref_volume = 0;
		Base_demand_volume = 0;
		for(int m = 1; m< MAX_MODE_TYPES-1; m++)
		{
		Obs_volume[m] = -1;
		mode_MainVolume[m] = 0;
		mode_MainVolume[m] = 0;
		mode_Base_demand_volume[m] = 0;
		mode_SubVolume[m] = 0;
		mode_SDVolume[m] = 0;
		mode_Toll[m] = 0; 
		mode_AdditionalCost[m] = 0;
		}
		Lane_SaturationFlowRate = 1800;

		timing_arc_flag = 0;
		cycle_length = 60;
		start_green_time = 0;
		end_green_time = 30; 
		background_volume = 0; 
		non_uturn_flag = 0;
		b_withmovement_restrictions = false;

	}
	void setup(int num_of_modes)
	{
		link_type = 1;
		VDF_Alpha = 0.15;
		VDF_Beta = 4;
		VDF_plf = 1;

		Q_fd = 1.0;
		Q_n = 1.0;
		Q_fp = 0.28125 /*0.15*15/8*/;
		Q_s = 4;

		Travel_time = 0;
		BPR_TT = 0;
		QVDF_TT = 0;
		Ref_volume = 0;
		Base_demand_volume = 0;

		for (int m = 1; m <= num_of_modes; m++)
		{
			Obs_volume[m] = -1;
			mode_MainVolume[m] = 0;
			mode_MainVolume[m] = 0;
			mode_Base_demand_volume[m] = 0;
			mode_SubVolume[m] = 0;
			mode_SDVolume[m] = 0;
			mode_Toll[m] = 0;
			mode_AdditionalCost[m] = 0;
		}
	}
};


// Define a 2D map: ib_link_id → ob_link_id → restricted (bool)
std::map<int, std::map<int, bool>> global_movement_restrictions;

// Function to insert a movement restriction
void InsertMovementRestriction(int ib_link_id, int ob_link_id, bool is_restricted)
{
	global_movement_restrictions[ib_link_id][ob_link_id] = is_restricted;
}

// Function to check if a movement restriction exists
bool IsMovementRestricted(int ib_link_id, int ob_link_id)
{
	if (global_movement_restrictions.find(ib_link_id) != global_movement_restrictions.end())
	{
		if (global_movement_restrictions[ib_link_id].find(ob_link_id) != global_movement_restrictions[ib_link_id].end())
		{
			return global_movement_restrictions[ib_link_id][ob_link_id];
		}
	}
	return false; // Default: no restriction
}
struct mode_type {
	std::string mode_type ;
	float vot;
	float pce;
	float occ;
	int dedicated_shortest_path;
	std::string demand_file;

		
};


mode_type g_mode_type_vector[MAX_MODE_TYPES];
int g_metric_system_flag = 1; 
void StatusMessage(const char* group, const char* format, ...);

struct link_record* Link;
int* FirstLinkFrom;  // used in shortet path algorithm 
int* LastLinkFrom;


double Link_GenCost(int k, double* Volume);
double LinkCost_Integral(int k, double* Volume);
double Link_GenCostDer(int k, double* Volume);

void* Alloc_1D(int dim1, size_t size);
void** Alloc_2D(int dim1, int dim2, size_t size);
void Free_2D(void** Array, int dim1, int dim2);
void*** Alloc_3D(int dim1, int dim2, int dim3, size_t size);
void Free_3D(void*** Array, int dim1, int dim2, int dim3);

/* LINK VECTOR FUNCTIONS */
void ClearVolume(double* VolumeArray);
void VolumeDifference(double* Volume1, double* Volume2, double* Difference);
void UpdateVolume(double* MainVolume, double* SubVolume, double Lambda);

/* LINK COST FUNCTIONS */
void UpdateLinkAdditionalCost(void);
double UpdateLinkCost(double* Volume);
void UpdateLinkCostDer(double* Volume);
void GetLinkTravelTimes(double* Volume, double* TravelTime);
double TotalLinkCost(double* Volume);

/*  LINK OBJECTIVE FUNCTION */
double OF_Links(double* MainVolume);
double OF_LinksDirectionalDerivative(double* MainVolume, double* SDVolume, double Lambda);

void InitLinks(void);
void CloseLinks(void);

int Read_ODflow(double* TotalODflow, int* number_of_modes, int* no_zones);

int Minpath(int m, int Orig, int* PredLink, double* Cost_to);
double FindMinCostRoutes(int** MinPathPredLink);
void All_or_Nothing_Assign(double** ODflow, int** MinPathPredLink, double* Volume);


double LinksSDLineSearch(double* MainVolume, double* SDVolume);


/* Gloabal variables */

int no_zones, number_of_modes, no_nodes, number_of_links, FirstThruNode;
int number_of_internal_zones = 1; 
int TotalAssignIterations = 20;
double demand_period_starting_hours = 7;
double	demand_period_ending_hours = 8;
int first_through_node_id_input = -1; 
int g_tap_log_file = 0;
int g_base_demand_mode = 1;
int g_ODME_mode = 0;
int g_ODME_target_od = -1;
double g_ODME_obs_VMT = -1;
double g_System_VMT = 0;

double g_ODME_link_volume_penalty = 0.1;  // relative weight on volume , convert the deviation of link volume to the travel time 
double g_ODME_VMT_penalty = 1e-6;  // relative weight on VMT , convert the deviation of VMT to the travel time 
double g_ODME_od_penalty = 0.01;  // relative weight on od , 
FILE* summary_log_file = NULL;


double** ODflow, TotalODflow;
double* TotalOFlow;
int* zone_outbound_link_size;

double*** seed_MDODflow;
double*** old_MDODflow;
double*** candidate_MDODflow;
double*** gradient_MDODflow;


double*** MDODflow;
double*** MDDiffODflow;  // D^c - D^b
double*** targetMDODflow; // for ODME

double*** MDRouteCost;
/* Local Declarations */
/* void FW(void); Should there be a function for fw, or should it be included in main? */
static void Init(int input_number_of_modes, int input_no_zones);
static void Close();
static void InitODflow(int input_number_of_modes, int input_no_zones);
static void CloseODflow(void);
/* End of local declarations. */

FILE* logfile;
int route_output_flag = 0;
int simulation_output_flag = 0; 
int vehicle_log_flag = 0;
int baseODDemand_loaded_flag = 0;
int baselinkvolume_loaded_flag = 0;

extern int SimulationAPI(); 

constexpr auto LCG_a = 17364;
constexpr auto LCG_c = 0;
constexpr auto LCG_M = 65521; // 16-bit memory-saving LCG
unsigned int RandomSeed = 12345; // Initial random seed

// Function to generate random numbers using LCG
float generate_random() {
	RandomSeed = (LCG_a * RandomSeed + LCG_c) % LCG_M;
	return float(RandomSeed) / LCG_M; // Normalize to [0, 1)
}




// Helper function to convert minutes to hh:mm:ss format
std::string convert_minutes_to_hhmmss(double minutes) {
	int total_seconds = static_cast<int>(minutes * 60); // Convert minutes to total seconds
	int hours = total_seconds / 3600;                   // Extract hours
	int remaining_seconds = total_seconds % 3600;       // Remaining seconds after hours
	int mins = remaining_seconds / 60;                  // Extract minutes
	int secs = remaining_seconds % 60;                  // Extract seconds

	// Format as hh:mm:ss
	std::ostringstream oss;
	oss << std::setw(2) << std::setfill('0') << hours << ":"
		<< std::setw(2) << std::setfill('0') << mins << ":"
		<< std::setw(2) << std::setfill('0') << secs;

	return oss.str();
}


// Additional functions (find_links_to, init_link_pointers, init_links) would follow
// with similar C++ implementations...
// Calculate departure time function
std::vector<double> calculate_departure_time(double T0, double T1, double T3, double beta, int route_volume) {
	std::vector<double> departure_times;
	double time_span = T3 - T0;

	for (int i = 0; i < route_volume; i++) {
		double normalized_time = static_cast<double>(i) / route_volume;
		double time = T0 + (beta * pow(normalized_time, 2) + normalized_time) * time_span;
		departure_times.push_back(time);
	}

	return departure_times;
}
FILE* link_performance_file;

#define INVALID -1      /* Represents an invalid value. */
#define BIGM 9999999      /* Represents an invalid value. */
#define WAS_IN_QUEUE -7 /* Shows that the node was in the queue before. (7 is for luck.) */

bool rewriteFile(const std::string& filename, const std::string& content) {
	// Optional: Check if content is empty and inform the user
	if (content.empty()) {
		std::cout << "The provided content is empty. The file will be cleared." << std::endl;
	}

	std::ofstream outputFile(filename);  // Open the file (truncates by default)

	if (!outputFile.is_open()) {
		std::cerr << "Error opening file: " << filename << std::endl;
		return false;
	}

	outputFile << content;  // Write the (empty) content to the file
	outputFile.close();     // Close the file
	return true;
}



double Link_QueueVDF(int k, double Volume, double& IncomingDemand, double& DOC, double& P, double& t0, double& t2, double& t3, double& vt2, double& Q_mu, double& Q_gamma, double& congestion_ref_speed,
	double& avg_queue_speed, double& avg_QVDF_period_speed, double& Severe_Congestion_P, double model_speed[300]);

class Node {
public:
	int node_id;
	int zone_id; 
	int internal_zone_no; 
	double x, y; 
	vector<int> m_incoming_link_seq_no_vector;
	vector<int> m_outgoing_link_seq_no_vector;

	Node() : node_id(-1) { zone_id = -1; internal_zone_no = -1; }
};

vector<Node> g_node_vector;

int Minpath(int mode, int Orig, int* PredLink, double* CostTo)  // CostTo is based on origin 
{
	int node, now, NewNode, k, Return2Q_Count = 0;
	double NewCost;
	int* QueueNext;
	int QueueFirst, QueueLast;
	int* PrevLink;

	QueueNext = (int*)Alloc_1D(no_nodes, sizeof(int));
	PrevLink = (int*)Alloc_1D(no_nodes, sizeof(int));

	for (node = 1; node <= no_nodes; node++)
	{
		QueueNext[node] = INVALID;
		CostTo[node] = BIGM;
		PredLink[node] = INVALID;
		PrevLink[node] = INVALID;
	}

	now = g_map_external_node_id_2_node_seq_no[Orig];
	int internal_node_id_for_origin_zone = now;
	QueueNext[now] = WAS_IN_QUEUE;
	PredLink[now] = INVALID;
	PrevLink[now] = INVALID;
	CostTo[now] = 0.0;

	QueueFirst = QueueLast = INVALID;

	while ((now != INVALID) && (now != WAS_IN_QUEUE))
	{
		if (now >= FirstThruNode || now == internal_node_id_for_origin_zone)
		{
			for (k = FirstLinkFrom[now]; k <= LastLinkFrom[now]; k++)
			{
				if (Link[k].mode_allowed_use[mode] == 0)
					continue;

				NewNode = Link[k].internal_to_node_id;
				NewCost = CostTo[now] + Link[k].Travel_time + Link[k].mode_AdditionalCost[mode];

				// Check if the previous link (PrevLink[now]) has restrictions on this link (k)
				if (PrevLink[now] != INVALID && Link[PrevLink[now]].b_withmovement_restrictions == true)
				{
					if(IsMovementRestricted(PrevLink[now],k))
						continue;  // Skip restricted movement
				}

				if (CostTo[NewNode] > NewCost)
				{
					CostTo[NewNode] = NewCost;
					PredLink[NewNode] = k;
					PrevLink[NewNode] = k;

					if (QueueNext[NewNode] == WAS_IN_QUEUE)
					{
						QueueNext[NewNode] = QueueFirst;
						QueueFirst = NewNode;
						if (QueueLast == INVALID)
							QueueLast = NewNode;
						Return2Q_Count++;
					}
					else if (QueueNext[NewNode] == INVALID && NewNode != QueueLast)
					{
						if (QueueLast != INVALID)
						{
							QueueNext[QueueLast] = NewNode;
							QueueLast = NewNode;
						}
						else
						{
							QueueFirst = QueueLast = NewNode;
							QueueNext[QueueLast] = INVALID;
						}
					}
				}
			}
		}

		now = QueueFirst;
		if ((now == INVALID) || (now == WAS_IN_QUEUE))
			break;

		QueueFirst = QueueNext[now];
		QueueNext[now] = WAS_IN_QUEUE;
		if (QueueLast == now)
			QueueLast = INVALID;
	}

	free(QueueNext);
	free(PrevLink);

	return (Return2Q_Count);
}


/* Find minimum cost routes .
Input: 	None
Output:	RouteCost - route generalized cost, by origin and destination
		MinPathSuccLink - trees of minimum cost routes, by destination and node. */
std::vector<int> Processor_origin_zones[50];

int g_number_of_processors = 4;

double FindMinCostRoutes(int*** MinPathPredLink)
{
	double** CostTo;

	CostTo = (double**)Alloc_2D(no_zones, no_nodes, sizeof(double));  // cost to is based on the zone number (original node numbers) and # of nodes (node sequence no.) 
	StatusMessage("Minpath", "Starting the minpath calculations.");
	double* system_least_travel_time_org_zone = (double*)Alloc_1D(no_zones, sizeof(double));

#pragma omp parallel for
	for (int p = 0; p < g_number_of_processors; p++)
	{

		for (int i = 0; i < Processor_origin_zones[p].size(); i++)
		{

		int Orig = Processor_origin_zones[p][i];  // get origin zone id


		if(TotalOFlow[Orig] < 0.00001)  // only work on positive zone flow 
			continue;

		system_least_travel_time_org_zone[Orig] = 0;  // reset it before mode based computing 

		for (int m = 1; m <= number_of_modes; m++)
		{
			if (g_mode_type_vector[m].dedicated_shortest_path == 0)  // skip the shortest path computing
				continue;

			Minpath(m, Orig, MinPathPredLink[m][Orig], CostTo[Orig]);
			if (MDRouteCost != NULL)
			{
				for (int Dest = 1; Dest <= no_zones; Dest++)
				{
					MDRouteCost[m][Orig][Dest] = BIGM; // initialization 

					if (MDODflow[m][Orig][Dest] > 0.000001)
					{
						int  internal_node_id_for_destination_zone = g_map_external_node_id_2_node_seq_no[Dest]; 
						if (CostTo[Orig][internal_node_id_for_destination_zone] < 0.0)
							ExitMessage("Negative cost %lg from Origin %d to Destination %d.",
								(double)CostTo[Orig][Dest], Orig, Dest);

						if (CostTo[Orig][internal_node_id_for_destination_zone] <= BIGM - 1)  // feasible cost 
						{
							MDRouteCost[m][Orig][Dest] = CostTo[Orig][internal_node_id_for_destination_zone];

							system_least_travel_time_org_zone[Orig] += MDRouteCost[m][Orig][Dest] * MDODflow[m][Orig][Dest] * g_mode_type_vector[m].pce;

						}
						else
						{
							int debug_flag = 1;
						}
					}
				}
			}
		}
	}

	}

	double system_least_travel_time = 0;
	for (int Orig = 1; Orig <= no_zones; Orig++)
	{
		system_least_travel_time += system_least_travel_time_org_zone[Orig];
	}


	// free(CostTo);
	Free_2D((void**)CostTo, no_zones, no_nodes);
	free(system_least_travel_time_org_zone);
	StatusMessage("Minpath", "Found all minpath.");
	return system_least_travel_time;
}

// Structure to hold path information for each OD pair
struct ODPathInfo {
	int mode = 0;
	double distance = -1;
	double freeFlowTime = 0.0;
	double congestionTime = 0.0;
	double cost = -1.0;
};




//#include <iomanip>

void WriteOutputFiles(const char* filename, ODPathInfo** odPathInfoMatrix)
{


	auto start = std::chrono::high_resolution_clock::now();
	std::ofstream outputFile(filename);
	if (!outputFile.is_open()) {
		std::cerr << "Error: Could not open output od_performance.csv file." << std::endl;
		return;
	}

	// Write headers (if needed)
	outputFile << "o_zone_id,d_zone_id,total_distance_mile,total_distance_km,total_free_flow_travel_time\n";

	// Global statistics initialization
	double grand_totalDistance = 0.0, grand_totalFreeFlowTravelTime = 0.0, grand_totalTravelTime = 0.0, grand_total_count = 0;

	// Set fixed precision to 2 decimal places to reduce file size
	outputFile << std::fixed << std::setprecision(3);

	for (int Orig = 0; Orig < number_of_internal_zones; ++Orig) {
		for (int Dest = 0; Dest < number_of_internal_zones; ++Dest) {
			if (Orig == Dest)
				continue;

			const ODPathInfo& pathInfo = odPathInfoMatrix[Orig][Dest];
			if (pathInfo.cost < 0.0)
				continue;

			int internal_o_node_id = g_map_internal_zone_no_2_node_seq_no[Orig];
			int internal_d_node_id = g_map_internal_zone_no_2_node_seq_no[Dest];
			float volume = 1;

			grand_totalDistance += pathInfo.distance * volume;
			grand_totalFreeFlowTravelTime += pathInfo.freeFlowTime * volume;
			grand_totalTravelTime += pathInfo.congestionTime * volume;
			grand_total_count += volume;

			outputFile << g_node_vector[internal_o_node_id].node_id << ","
				<< g_node_vector[internal_d_node_id].node_id << ","
				<< pathInfo.distance << "," << (pathInfo.distance * 1.609) << ","
				<< pathInfo.freeFlowTime << "\n";
		}
	}

	outputFile.close();
	std::cout << "Output written to " << filename << std::endl;

	if (grand_total_count > 0) {
		std::cout << "---------- Summary Statistics ----------\n"
			<< "Average path distance: " << grand_totalDistance / grand_total_count << " miles\n"
			<< "Average free flow travel time: " << grand_totalFreeFlowTravelTime / grand_total_count << " minutes\n";
	}

	// Calculate elapsed time
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = end - start;
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration % std::chrono::seconds(1));

	printf("Printing out OD accessibility: %lld hours %lld minutes %lld seconds %lld ms\n",
		hours.count(), minutes.count(), seconds.count(), milliseconds.count());
}


// Aggregated statistics for a zone
struct ZoneAggregates {
	double totalDistance = 0.0;
	double totalFreeFlowTime = 0.0;
	double totalCongestionTime = 0.0;
	int count = 0;
};


void WriteZoneAccessibilityCSV(const char* filename, ODPathInfo** odPathInfoMatrix)
{
	auto start = std::chrono::high_resolution_clock::now();

	std::ofstream outputFile(filename);
	if (!outputFile.is_open()) {
		std::cerr << "Error: Could not open output zone_accessibility.csv file." << std::endl;
		return;
	}

	// Write header line.
	// Columns:
	// zone_id,
	// origin_count, origin_avg_distance_mile, origin_avg_distance_km, origin_avg_free_flow, origin_avg_congestion,
	// destination_count, destination_avg_distance_mile, destination_avg_distance_km, destination_avg_free_flow, destination_avg_congestion
	outputFile << "zone_id,origin_count,origin_avg_distance_mile,origin_avg_distance_km,origin_avg_free_flow,origin_avg_congestion,"
		<< "destination_count,destination_avg_distance_mile,destination_avg_distance_km,destination_avg_free_flow,destination_avg_congestion\n";

	// Create aggregation vectors (one element per zone).
	std::vector<ZoneAggregates> originAgg(number_of_internal_zones);
	std::vector<ZoneAggregates> destAgg(number_of_internal_zones);

	// Process every OD pair.
	for (int Orig = 0; Orig < number_of_internal_zones; ++Orig) {
		for (int Dest = 0; Dest < number_of_internal_zones; ++Dest) {
			if (Orig == Dest)
				continue;  // Skip self trips.
			const ODPathInfo& pathInfo = odPathInfoMatrix[Orig][Dest];
			if (pathInfo.cost < 0.0)
				continue;  // Skip unreachable pairs.

			// Update Origin aggregates for zone Orig.
			originAgg[Orig].totalDistance += pathInfo.distance;
			originAgg[Orig].totalFreeFlowTime += pathInfo.freeFlowTime;
			originAgg[Orig].totalCongestionTime += pathInfo.congestionTime;
			originAgg[Orig].count++;

			// Update Destination aggregates for zone Dest.
			destAgg[Dest].totalDistance += pathInfo.distance;
			destAgg[Dest].totalFreeFlowTime += pathInfo.freeFlowTime;
			destAgg[Dest].totalCongestionTime += pathInfo.congestionTime;
			destAgg[Dest].count++;
		}
	}

	// Set fixed precision for output numbers.
	outputFile << std::fixed << std::setprecision(2);

	// Write out the aggregated values for each zone.
	for (int zoneIdx = 0; zoneIdx < number_of_internal_zones; ++zoneIdx) {
		// Map the internal zone index to a node (zone) id.
		int nodeIdx = g_map_internal_zone_no_2_node_seq_no[zoneIdx];
		int zone_id = g_node_vector[nodeIdx].node_id;

		// Origin measures.
		int origCount = originAgg[zoneIdx].count;
		double origAvgDistance = (origCount > 0) ? (originAgg[zoneIdx].totalDistance / origCount) : 0.0;
		double origAvgDistanceKM = origAvgDistance * 1.609;
		double origAvgFreeFlow = (origCount > 0) ? (originAgg[zoneIdx].totalFreeFlowTime / origCount) : 0.0;
		double origAvgCongestion = (origCount > 0) ? (originAgg[zoneIdx].totalCongestionTime / origCount) : 0.0;

		// Destination measures.
		int destCount = destAgg[zoneIdx].count;
		double destAvgDistance = (destCount > 0) ? (destAgg[zoneIdx].totalDistance / destCount) : 0.0;
		double destAvgDistanceKM = destAvgDistance * 1.609;
		double destAvgFreeFlow = (destCount > 0) ? (destAgg[zoneIdx].totalFreeFlowTime / destCount) : 0.0;
		double destAvgCongestion = (destCount > 0) ? (destAgg[zoneIdx].totalCongestionTime / destCount) : 0.0;

		// Write CSV row.
		outputFile << zone_id << ","
			<< origCount << "," << origAvgDistance << "," << origAvgDistanceKM << "," << origAvgFreeFlow << "," << origAvgCongestion << ","
			<< destCount << "," << destAvgDistance << "," << destAvgDistanceKM << "," << destAvgFreeFlow << "," << destAvgCongestion << "\n";
	}

	outputFile.close();
	std::cout << "Zone-based accessibility output written to " << filename << std::endl;

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = end - start;
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration % std::chrono::seconds(1));

	printf("Elapsed time: %lld hours %lld minutes %lld seconds %lld ms\n",
		hours.count(), minutes.count(), seconds.count(), milliseconds.count());
}
// Function to compute accessibility and OD costs
int ComputeAccessibilityAndODCosts_v1(const char* filename)
{
	// Start timing
	auto start0 = std::chrono::high_resolution_clock::now();

	// Allocate memory for cost matrix
	double** CostTo = (double**)Alloc_2D(50, no_nodes+1, sizeof(double));



	for (int k = 1; k <= number_of_links; k++)
		Link[k].Travel_time = Link[k].FreeTravelTime;

	// Store the link sequences for each OD pair
	// Simplified to 2D vector: first dimension is origin zone, second is destination zone

	ODPathInfo** odPathInfoMatrix = (ODPathInfo**)Alloc_2D(number_of_internal_zones, number_of_internal_zones, sizeof(ODPathInfo));

	// Allocate memory for predecessor links
	int** PredLink = (int**)Alloc_2D(50, no_nodes + 1, sizeof(int));

	cout << " Memory allocation completes. Starting the minpath calculations." << endl; 

	// Parallel processing of origin zones
#pragma omp parallel for
	for (int p = 0; p < g_number_of_processors; p++)
	{
		for (int i = 0; i < Processor_origin_zones[p].size(); i++)
		{
			int Orig = Processor_origin_zones[p][i];  // get origin zone id

			if (p == 0)
				cout << "Accessibility computing for zone " << Orig << endl;

			// Process each mode
			for (int m = 1; m <= number_of_modes; m++)
			{
				// Compute shortest path from origin to all destinations
				Minpath(m, Orig, PredLink[p], CostTo[p]);

				// Store path information for each destination
				for (int j = 1; j <= no_nodes; j++)
				{
					if(g_node_vector[j].zone_id >0)
					{
						int Dest = g_node_vector[j].zone_id;
						if (CostTo[p][j] < BIGM - 1)
						{
							// Calculate path metrics
							double pathDistance = 0.0;
							double freeFlowTime = 0.0;
							double congestionTime = 0.0;

							int currentNode = g_map_external_node_id_2_node_seq_no[Dest];

							while (currentNode != Orig && currentNode != 0)
							{
								int linkId = PredLink[p][currentNode];
								if (linkId == -1)
									break;
								pathDistance += Link[linkId].length;
								freeFlowTime += Link[linkId].FreeTravelTime;
								congestionTime += Link[linkId].Travel_time;
								currentNode = Link[linkId].internal_from_node_id;
							}

							int internal_node_id = g_map_external_node_id_2_node_seq_no[Orig];
							int internal_o_zone_no = g_node_vector[internal_node_id].internal_zone_no;
							int internal_d_zone_no = g_node_vector[j].internal_zone_no;
							// Store in our 2D matrix
							ODPathInfo& pathInfo = odPathInfoMatrix[internal_o_zone_no][internal_d_zone_no];
							pathInfo.mode = m;
							pathInfo.distance = pathDistance;
							pathInfo.freeFlowTime = freeFlowTime;
							pathInfo.congestionTime = congestionTime;
							pathInfo.cost = CostTo[p][Dest];
						}
						else
						{
							int internal_node_id = g_map_external_node_id_2_node_seq_no[Orig];
							int internal_o_zone_no = g_node_vector[internal_node_id].internal_zone_no;
							int internal_d_zone_no = g_node_vector[j].internal_zone_no;
							// Store in our 2D matrix
							ODPathInfo& pathInfo = odPathInfoMatrix[internal_o_zone_no][internal_d_zone_no];
							pathInfo.mode = m;
							pathInfo.distance = -1;
							pathInfo.freeFlowTime = -1;
							pathInfo.congestionTime = -1;
							pathInfo.cost = -1;
						}
					}
				}
			}
		}
	}

	// Free the memory for costs and predecessor links
	Free_2D((void**)CostTo, 50, no_nodes+1);
	Free_2D((void**)PredLink, 50, no_nodes+1);

	// Calculate elapsed time
	auto end0 = std::chrono::high_resolution_clock::now();
	auto duration = end0 - start0;

	// Convert to hours, minutes, seconds
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration % std::chrono::seconds(1));

	printf("All OD accessibility computing: %lld hours %lld minutes %lld seconds %lld ms\n",
		hours.count(), minutes.count(), seconds.count(), milliseconds.count());

	// Output the results to files

	if(number_of_internal_zones<10000)
		WriteOutputFiles(filename, odPathInfoMatrix);

	WriteZoneAccessibilityCSV("zone_accessibility.csv", odPathInfoMatrix);

	Free_2D((void**)odPathInfoMatrix, number_of_internal_zones, number_of_internal_zones);

	return 1;
}

// Function to compute accessibility and OD costs
int ComputeAccessibilityAndODCosts_v2(const char* filename)
{
	auto start0 = std::chrono::high_resolution_clock::now();

	// Allocate cost and predecessor arrays (assumed size 50 is for processor count)
	double** CostTo = (double**)Alloc_2D(50, no_nodes + 1, sizeof(double));
	int** PredLink = (int**)Alloc_2D(50, no_nodes + 1, sizeof(int));



	// Set each link's travel time to its free travel time.
	for (int k = 1; k <= number_of_links; k++) {
		Link[k].Travel_time = Link[k].FreeTravelTime;
	}

	// Prepare aggregation vectors for zone-based statistics.
	std::vector<ZoneAggregates> originAgg(number_of_internal_zones);
	std::vector<ZoneAggregates> destAgg(number_of_internal_zones);

	// Open the OD performance output file and write header.
	std::ofstream outputFile(filename);
	if (!outputFile.is_open()) {
		std::cerr << "Error: Could not open output file " << filename << std::endl;
		return -1;
	}
	outputFile << "o_zone_id,d_zone_id,total_distance_mile,total_distance_km,total_free_flow_travel_time\n";
	outputFile << std::fixed << std::setprecision(3);

	// Define the batch size. For example, process 100 origin zones at a time.
	const int batch_size = 100;
	// Loop over origin zones in batches.
	for (int batchStart = 0; batchStart < number_of_internal_zones; batchStart += batch_size) {

		cout << "outputing accessibility for zone id: \n" << batchStart + 1 << endl;

		int currentBatchSize = std::min(batch_size, number_of_internal_zones - batchStart);

		// Allocate a temporary OD matrix for this batch:
		// Each row corresponds to an origin in [batchStart, batchStart + currentBatchSize)
		// and each column corresponds to a destination (internal zone index).
		ODPathInfo** batchMatrix = (ODPathInfo**)Alloc_2D(currentBatchSize, number_of_internal_zones, sizeof(ODPathInfo));

		// Initialize the batchMatrix.
		for (int i = 0; i < currentBatchSize; ++i) {
			for (int j = 0; j < number_of_internal_zones; ++j) {
				batchMatrix[i][j].mode = 0;
				batchMatrix[i][j].distance = -1;
				batchMatrix[i][j].freeFlowTime = 0.0;
				batchMatrix[i][j].congestionTime = 0.0;
				batchMatrix[i][j].cost = -1.0;
			}
		}

		// Process each origin zone in the current batch.
		for (int localOrigin = 0; localOrigin < currentBatchSize; ++localOrigin) {
			int Orig = batchStart + localOrigin;

			// Process each transportation mode.
			for (int m = 1; m <= number_of_modes; m++) {
				// Compute shortest path from origin Orig to all destinations.
				// (For simplicity, we use processor index 0. Adapt if you are using parallel processing.)
				Minpath(m, Orig, PredLink[0], CostTo[0]);

				// Process each destination node.
				for (int j = 1; j <= no_nodes; j++) {
					if (g_node_vector[j].zone_id > 0) {
						int Dest = g_node_vector[j].zone_id;
						if (CostTo[0][j] < BIGM - 1) {
							double pathDistance = 0.0;
							double freeFlowTime = 0.0;
							double congestionTime = 0.0;

							int currentNode = g_map_external_node_id_2_node_seq_no[Dest];
							// Backtrack using predecessor links to accumulate path metrics.
							while (currentNode != Orig && currentNode != 0) {
								int linkId = PredLink[0][currentNode];
								if (linkId == -1)
									break;
								pathDistance += Link[linkId].length;
								freeFlowTime += Link[linkId].FreeTravelTime;
								congestionTime += Link[linkId].Travel_time;
								currentNode = Link[linkId].internal_from_node_id;
							}

							int internal_node_id = g_map_external_node_id_2_node_seq_no[Orig];
							int internal_o_zone_no = g_node_vector[internal_node_id].internal_zone_no;
							int internal_d_zone_no = g_node_vector[j].internal_zone_no;

							// Update batchMatrix for this origin-destination pair.
							ODPathInfo& pathInfo = batchMatrix[localOrigin][internal_d_zone_no];
							pathInfo.mode = m;
							pathInfo.distance = pathDistance;
							pathInfo.freeFlowTime = freeFlowTime;
							pathInfo.congestionTime = congestionTime;
							pathInfo.cost = CostTo[0][j];

							// Update aggregation if the path is reachable.
							if (pathInfo.cost >= 0.0) {
								originAgg[internal_o_zone_no].totalDistance += pathDistance;
								originAgg[internal_o_zone_no].totalFreeFlowTime += freeFlowTime;
								originAgg[internal_o_zone_no].totalCongestionTime += congestionTime;
								originAgg[internal_o_zone_no].count++;

								destAgg[internal_d_zone_no].totalDistance += pathDistance;
								destAgg[internal_d_zone_no].totalFreeFlowTime += freeFlowTime;
								destAgg[internal_d_zone_no].totalCongestionTime += congestionTime;
								destAgg[internal_d_zone_no].count++;
							}
						}
						else {
							// Unreachable destination.
							int internal_node_id = g_map_external_node_id_2_node_seq_no[Orig];
							int internal_o_zone_no = g_node_vector[internal_node_id].internal_zone_no;
							int internal_d_zone_no = g_node_vector[j].internal_zone_no;
							ODPathInfo& pathInfo = batchMatrix[localOrigin][internal_d_zone_no];
							pathInfo.mode = m;
							pathInfo.distance = -1;
							pathInfo.freeFlowTime = -1;
							pathInfo.congestionTime = -1;
							pathInfo.cost = -1;
						}
					}
				} // End for each destination
			} // End for each mode

			// After processing all modes for the current origin, write its OD data.
			for (int Dest = 0; Dest < number_of_internal_zones; ++Dest) {
				ODPathInfo& info = batchMatrix[localOrigin][Dest];
				// Skip self-trips and unreachable pairs.
				if (info.cost < 0 || Orig == Dest)
					continue;
				int internal_o_node_id = g_map_internal_zone_no_2_node_seq_no[Orig];
				int internal_d_node_id = g_map_internal_zone_no_2_node_seq_no[Dest];
				outputFile << g_node_vector[internal_o_node_id].node_id << ","
					<< g_node_vector[internal_d_node_id].node_id << ","
					<< info.distance << "," << (info.distance * 1.609) << ","
					<< info.freeFlowTime << "\n";
			}
		} // End for each origin in batch

		// Free the temporary batch matrix.
		Free_2D((void**)batchMatrix, currentBatchSize, number_of_internal_zones);
	} // End batch loop

	// Free the cost and predecessor arrays.
	Free_2D((void**)CostTo, 50, no_nodes + 1);
	Free_2D((void**)PredLink, 50, no_nodes + 1);

	// Write the zone accessibility aggregated statistics to a separate CSV file.
	std::ofstream zoneFile("zone_accessibility.csv");
	if (!zoneFile.is_open()) {
		std::cerr << "Error: Could not open zone_accessibility.csv file." << std::endl;
		return -1;
	}


		cout << "outputing zone_accessibility.csv file." << std::endl;

		zoneFile << "zone_id,origin_count,origin_avg_distance_mile,origin_avg_distance_km,origin_avg_free_flow,origin_avg_congestion,"
		<< "destination_count,destination_avg_distance_mile,destination_avg_distance_km,destination_avg_free_flow,destination_avg_congestion\n";
	zoneFile << std::fixed << std::setprecision(2);
	for (int zoneIdx = 0; zoneIdx < number_of_internal_zones; zoneIdx++) {
		int nodeIdx = g_map_internal_zone_no_2_node_seq_no[zoneIdx];
		int zone_id = g_node_vector[nodeIdx].node_id;

		int origCount = originAgg[zoneIdx].count;
		double origAvgDistance = (origCount > 0) ? (originAgg[zoneIdx].totalDistance / origCount) : 0.0;
		double origAvgDistanceKM = origAvgDistance * 1.609;
		double origAvgFreeFlow = (origCount > 0) ? (originAgg[zoneIdx].totalFreeFlowTime / origCount) : 0.0;
		double origAvgCongestion = (origCount > 0) ? (originAgg[zoneIdx].totalCongestionTime / origCount) : 0.0;

		int destCount = destAgg[zoneIdx].count;
		double destAvgDistance = (destCount > 0) ? (destAgg[zoneIdx].totalDistance / destCount) : 0.0;
		double destAvgDistanceKM = destAvgDistance * 1.609;
		double destAvgFreeFlow = (destCount > 0) ? (destAgg[zoneIdx].totalFreeFlowTime / destCount) : 0.0;
		double destAvgCongestion = (destCount > 0) ? (destAgg[zoneIdx].totalCongestionTime / destCount) : 0.0;

		zoneFile << zone_id << ","
			<< origCount << "," << origAvgDistance << "," << origAvgDistanceKM << "," << origAvgFreeFlow << "," << origAvgCongestion << ","
			<< destCount << "," << destAvgDistance << "," << destAvgDistanceKM << "," << destAvgFreeFlow << "," << destAvgCongestion << "\n";
	}
	zoneFile.close();

	auto end0 = std::chrono::high_resolution_clock::now();
	auto duration = end0 - start0;
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration % std::chrono::seconds(1));

	printf("All OD accessibility computing: %lld hours %lld minutes %lld seconds %lld ms\n",
		hours.count(), minutes.count(), seconds.count(), milliseconds.count());

	return 1;
}

/* Assign OD flows to links according to the routes in MinPathPredLink. */

// Define a global 3D vector to store link indices for each OD pair

// Global 5D vector for storing link sequences
std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>> linkIndices;

void AddLinkSequence(int m, int Orig, int Dest, int route_id, const std::vector<int>& linkIDs)
{
	if (linkIndices.size() == 0)
		return; 

	// Ensure we are within bounds before adding the link sequence
	if (Orig > 0 && Orig < linkIndices[m].size() &&
		Dest > 0 && Dest < linkIndices[m][Orig].size() &&
		route_id >= 0 && route_id < linkIndices[m][Orig][Dest].size())
	{
		linkIndices[m][Orig][Dest][route_id] = linkIDs;  // Add link sequence to the 5D vector
	}
	else
	{
		std::cerr << "Error: Invalid indices for adding link sequence." << std::endl;
	}
}





void All_or_Nothing_Assign(int Assignment_iteration_no, double*** ODflow, int*** MinPathPredLink, double* Volume)
{
//	printf("All or nothing assignment\n");

	auto start0 = std::chrono::high_resolution_clock::now();
	double** ProcessorVolume;
	double*** ProcessorModeVolume;


	ProcessorVolume = (double**)Alloc_2D(number_of_links, g_number_of_processors, sizeof(double));

	if (ProcessorVolume == nullptr) {
		std::cerr << "Error: Memory allocation for ProcessorVolume failed." << std::endl;
		// Handle the error, e.g., exit the program, clean up resources, etc.
		exit(EXIT_FAILURE);
	}

	ProcessorModeVolume = (double***)Alloc_3D(number_of_links, number_of_modes, g_number_of_processors, sizeof(double));
	if (ProcessorModeVolume == nullptr) {
		std::cerr << "Error: Memory allocation for ProcessorModeVolume failed." << std::endl;
		// Handle the error, e.g., exit the program, clean up resources, etc.
		exit(EXIT_FAILURE);
	}

#pragma omp parallel for
	for (int k = 1; k <= number_of_links; k++)
	{


		for (int p = 0; p < g_number_of_processors; p++)
		{
			ProcessorVolume[k][p] = 0;

			for (int m = 1; m <= number_of_modes; m++)
				ProcessorModeVolume[k][m][p] = 0;
		}
	}
	//StatusMessage("Assign", "Starting assign.");

	if(Assignment_iteration_no == 0)
	{
		printf("The list of zero-volume zones:");
		for (int Orig = 1; Orig <= no_zones; Orig++)
		{
			
			if (g_map_external_node_id_2_node_seq_no.find(Orig) != g_map_external_node_id_2_node_seq_no.end() && TotalOFlow[Orig] < 0.00001)  // only work on positive zone flow 
			{ 
				printf("%d,", Orig);
			}

		}

		printf("\n");

		printf("The list of zones without outbound connecting links:");
		for (int Orig = 1; Orig <= no_zones; Orig++)
		{

			if (g_map_external_node_id_2_node_seq_no.find(Orig) != g_map_external_node_id_2_node_seq_no.end()  && zone_outbound_link_size[Orig] == 0)  // there is no outbound link from the origin 
			{
				printf("%d,", Orig);
			}
		}
		printf("\n");
	}
#pragma omp parallel for
	for (int p = 0; p < g_number_of_processors; p++)
	{

		for (int i = 0; i < Processor_origin_zones[p].size(); i++)
		{

			int Orig = Processor_origin_zones[p][i];  // get origin zone id
			
			if (TotalOFlow[Orig] < 0.00001)  // only work on positive zone flow 
				continue;

			if (zone_outbound_link_size[Orig] == 0)  // there is no outbound link from the origin 
				continue; 


			int Dest, k;
			int CurrentNode;
			double RouteFlow;
			std::vector<int> currentLinkSequence; // Temporary vector to store link indices


			for (int m = 1; m <= number_of_modes; m++)
			{


				//	printf("Assign", "Assigning origin %6d.", Orig);
				for (Dest = 1; Dest <= no_zones; Dest++)  // Dest zone 
				{
					if (Dest == Orig)
						continue;

					if (route_output_flag || Assignment_iteration_no == 0)
						currentLinkSequence.clear();

					RouteFlow = ODflow[m][Orig][Dest];  //test
					if (RouteFlow == 0)
						continue;

					if (g_mode_type_vector[m].dedicated_shortest_path == 0)  // skip the shortest path computing
					{
						if (MDRouteCost[1][Orig][Dest] >= BIGM - 1)  // if feasible 
							continue;
					}
					else
					{
						if (MDRouteCost[m][Orig][Dest] >= BIGM - 1)  // if feasible 
							continue;
					}

					CurrentNode = Dest;
					CurrentNode = g_map_external_node_id_2_node_seq_no[Dest];  // mapping from external  zone id of Dest (which is defined in demand.csv_ to the corresponding node id (== zone_id) and then to the node internal number 
					int internal_node_for_origin_node = g_map_external_node_id_2_node_seq_no[Orig];  // mapping from external  zone id of Orig (which is defined in demand.csv_ to the corresponding node id (== zone_id) and then to the node internal number 
					// MinPathPredLink is coded as internal node id 
					// 
					//double total_travel_time = 0;
					//double total_length = 0;
					//double total_FFTT = 0;

					while (CurrentNode != internal_node_for_origin_node)
					{
						if (g_mode_type_vector[m].dedicated_shortest_path == 0)  // skip the shortest path computing
							k = MinPathPredLink[1][Orig][CurrentNode];  // default to mode 1
						else
							k = MinPathPredLink[m][Orig][CurrentNode];

						if (k <= 0 || k > number_of_links || k == INVALID)
						{
							printf("A problem in All_or_Nothing_Assign() Invalid pred for node seq no %d Orig zone= %d \n\n", CurrentNode, Orig);
							break;
						}
						if (ProcessorVolume[k][p] >= 0.5 && RouteFlow >= 0.5)
							int debug_flag = 1;

						ProcessorVolume[k][p] += RouteFlow * g_mode_type_vector[m].pce;
						ProcessorModeVolume[k][m][p] += RouteFlow;  // pure volume 

						CurrentNode = Link[k].internal_from_node_id;

						if (CurrentNode <= 0 || CurrentNode > no_nodes )
						{
							printf("A problem in All_or_Nothing_Assign() Invalid node seq no %d Orig zone = %d \n\n", CurrentNode, Orig);
							break;
						}

						if(linkIndices.size() >0)
						{
						if (route_output_flag || Assignment_iteration_no == 0)
						{
#pragma omp critical
							{
								currentLinkSequence.push_back(k); // Store the link index
							}

						}
					}

							if (linkIndices.size() > 0)
							{
								if (route_output_flag || Assignment_iteration_no == 0)
								{
									int origin_seq_no = g_map_external_node_id_2_node_seq_no[Orig];
									int destination_seq_no = g_map_external_node_id_2_node_seq_no[Dest];

									AddLinkSequence(m, origin_seq_no, destination_seq_no, Assignment_iteration_no, currentLinkSequence);
									// Store the link sequence for this OD pair

								}
						}
						}

				}
			}

		}


	}

	if (Assignment_iteration_no == 0)  // with base demand
	{
		for (int k = 1; k <= number_of_links; k++)
		{
			Volume[k] = Link[k].Base_demand_volume;

			for (int p = 0; p < g_number_of_processors; p++)
			{
				Volume[k] += ProcessorVolume[k][p];
			}

			for (int m = 1; m <= number_of_modes; m++)
			{
				Link[k].mode_MainVolume[m] = Link[k].mode_Base_demand_volume[m];

				for (int p = 0; p < g_number_of_processors; p++)
				{
					Link[k].mode_MainVolume[m] += ProcessorModeVolume[k][m][p];
				}
			}

		}

	}
	else
	{  // all or nothing 
#pragma omp parallel for
		for (int k = 1; k <= number_of_links; k++)
		{
			Volume[k] = 0;

			for (int p = 0; p < g_number_of_processors; p++)
			{
				Volume[k] += ProcessorVolume[k][p];
			}

			for (int m = 1; m <= number_of_modes; m++)
			{
				Link[k].mode_SubVolume[m] = 0.0;

				for (int p = 0; p < g_number_of_processors; p++)
				{
					Link[k].mode_SubVolume[m] += ProcessorModeVolume[k][m][p];
				}
			}

		}
	}


	Free_2D((void**)ProcessorVolume, number_of_links, g_number_of_processors);
	Free_3D((void***)ProcessorModeVolume, number_of_links, number_of_modes, g_number_of_processors);

	auto end0 = std::chrono::high_resolution_clock::now();

	// Calculate the duration in milliseconds

	// Calculate the duration in seconds
	auto duration = end0 - start0;

	// Convert to hours, minutes, seconds
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration % std::chrono::seconds(1));

	printf("All or nothing assignment: %lld hours %lld minutes %lld seconds %lld ms\n", hours.count(), minutes.count(), seconds.count(), milliseconds.count());

}


//---------------------------------------------------------------------------
// ODME via Gradient Descent
//---------------------------------------------------------------------------
std::vector<double> computeTheta(const std::vector<double>& lambda) {
	// Start with the initial path which has a proportion of 1.
	std::vector<double> theta;
	theta.push_back(1.0);

	// For each iteration, update the current theta values and add the new path's proportion.
	for (size_t k = 0; k < lambda.size(); k++) {
		// Scale all current proportions by (1 - lambda[k]).
		for (size_t i = 0; i < theta.size(); i++) {
			theta[i] *= (1.0 - lambda[k]);
		}
		// Append the new path's proportion, which is lambda[k].
		theta.push_back(lambda[k]);
	}

	return theta;
}



void performODME(std::vector<double> theta, double* MainVolume, struct link_record* Link)
{
    // Open log file (this will overwrite any existing file with the same name)
    std::ofstream logFile("ODME_log.txt");
    if (!logFile.is_open()) {
        std::cerr << "Error: Could not open ODME_log.txt for writing." << std::endl;
        return;
    }
    logFile << "Starting ODME process.\n";

    // Define target and weights
    double VMT_target = g_ODME_obs_VMT;
    double w_link = g_ODME_link_volume_penalty;
    double w_od = g_ODME_od_penalty;
    double w_vmt = g_ODME_VMT_penalty;

    // Gradient descent and line search parameters
    double step_size = 0.2;   // initial step size guess
    int maxIter = 400;
    double tol = 1;
    const double armijo_c = 0.05; // Armijo condition constant

    // Get the number of modes and zones (note: index 0 is unused)
    int numModes = number_of_modes;
    int numZones = no_zones;

    // --------------------------------------------------
    // Helper lambda to compute the overall objective function.
    // The objective is the sum of:
    //   - OD demand deviations: w_od * (MDODflow - targetMDODflow)^2,
    //   - Link flow deviations: w_link * (linkFlow - Link[l].Obs_volume)^2,
    //   - VMT deviations: w_vmt * (VMT - VMT_target)^2.
	auto computeObjectiveForMDOD = [&](double*** mdod, double& r2) -> double {
		// Compute link flows from the given OD matrix "mdod"
		std::vector<double> linkFlows(number_of_links + 1, 0.0);
		for (int m = 1; m <= numModes; ++m) {
			for (int Orig = 1; Orig <= numZones; ++Orig) {
				for (int Dest = 1; Dest <= numZones; ++Dest) {
					if (Orig == Dest)
						continue;
					float od_flow = mdod[m][Orig][Dest];
					if (od_flow <= 0)
						continue;
					// Use available routes (assume equal split)
					int nRoutes = std::min(theta.size(), linkIndices[m][Orig][Dest].size());
					for (int route_id = 0; route_id < nRoutes; route_id++) {
						float routeFlow = od_flow * theta[route_id];
						if (linkIndices[m][Orig][Dest][route_id].empty())
							continue;
						for (int link_id : linkIndices[m][Orig][Dest][route_id]) {
							if (link_id >= 0 && link_id < static_cast<int>(linkFlows.size()))
								linkFlows[link_id] += routeFlow;
						}
					}
				}
			}
		}

		// Compute total VMT
		double VMT = 0.0;
		for (size_t l = 1; l < linkFlows.size(); ++l) {
			VMT += linkFlows[l] * Link[l].length;
		}

		// Sum up the objective terms
		double obj = 0.0;

		// OD demand term:
		for (int m = 1; m <= numModes; ++m) {
			for (int Orig = 1; Orig <= numZones; ++Orig) {
				for (int Dest = 1; Dest <= numZones; ++Dest) {
					if (Orig == Dest)
						continue;
					double diff = mdod[m][Orig][Dest] - targetMDODflow[m][Orig][Dest];
					obj += w_od * diff * diff;
				}
			}
		}

		// For R² calculation, first compute the mean of observed link volumes (for valid links)
		double sumObs = 0.0;
		int validCount = 0;
		for (size_t l = 1; l < linkFlows.size(); ++l) {
			if (Link[l].Obs_volume[1] > 1) {
				sumObs += Link[l].Obs_volume[1];
				validCount++;
			}
		}
		double meanObs = (validCount > 0) ? (sumObs / validCount) : 0.0;

		// Compute SSE and SStot for the link flow term
		double SSE_link = 0.0;
		double SStot = 0.0;
		for (size_t l = 1; l < linkFlows.size(); ++l) {
			if (Link[l].Obs_volume[1] > 1) {
				double diff = linkFlows[l] - Link[l].Obs_volume[1];
				SSE_link += w_link * diff * diff;
				double diffMean = Link[l].Obs_volume[1] - meanObs;
				SStot += diffMean * diffMean;
				// Also add to the overall objective function:
				obj += w_link * diff * diff;
			}
		}
		// Calculate R² (coefficient of determination) for link flows
		if (SStot > 0)
			r2 = 1.0 - SSE_link / SStot;
		else
			r2 = 1.0; // if SStot is 0, then the data is constant or perfect

		// VMT term:
		if (VMT_target > 1) {
			double diff = VMT - VMT_target;
			obj += w_vmt * diff * diff;
		}

		return obj;
		};

    // --------------------------------------------------
    // Main gradient descent iterations with line search
	double F_current_best = 0; 


	int count_Armijo_condition_NOT_met = 0; 
	double r2 = 0; 
	for (int iter = 0; iter < maxIter; ++iter) {

		// Compute the current objective value
		double F_current = computeObjectiveForMDOD(MDODflow, r2);
		logFile << "--------------------------------------------------\n";
//		cout << "Iteration " << iter << " R2 = " << r2 << "\n";

		if (iter == 0)
			F_current_best = F_current;
		else
		{
			if (F_current < F_current_best)
				F_current_best = F_current;

		}

		logFile << "Current objective = " << F_current << "\n";
		std::vector<double> linkFlows(number_of_links + 1, 0.0);
		// --- Log simple deviation measures ---

			// OD deviation: compute sum of absolute differences (and average)

		double totalODDev = 0.0;
		int odCount = 0;
		if (g_ODME_target_od >= 1)
		{
			for (int m = 1; m <= numModes; ++m) {
				for (int Orig = 1; Orig <= numZones; ++Orig) {
					for (int Dest = 1; Dest <= numZones; ++Dest) {
						if (Orig == Dest)
							continue;
						double diff = MDODflow[m][Orig][Dest] - targetMDODflow[m][Orig][Dest];
						totalODDev += diff;
						odCount++;
					}
				}
			}
		}
		double avgODDev = (odCount > 0 ? totalODDev / odCount : 0.0);

		// Link deviation: compute link flows and compare with observed volume

		for (int m = 1; m <= numModes; ++m) {
			for (int Orig = 1; Orig <= numZones; ++Orig) {
				for (int Dest = 1; Dest <= numZones; ++Dest) {
					if (Orig == Dest)
						continue;
					float od_flow = MDODflow[m][Orig][Dest];
					if (od_flow <= 0)
						continue;
					int nRoutes = std::min(theta.size(), linkIndices[m][Orig][Dest].size());
					for (int route_id = 0; route_id < nRoutes; route_id++) {
						float routeFlow = od_flow * theta[route_id];
						if (linkIndices[m][Orig][Dest][route_id].empty())
							continue;
						for (int link_id : linkIndices[m][Orig][Dest][route_id]) {
							if (link_id >= 0 && link_id < static_cast<int>(linkFlows.size()))
								linkFlows[link_id] += routeFlow;
						}
					}
				}
			}
		}
		double totalLinkDev = 0.0;
		int linkCount = 0;
		for (size_t l = 0; l < linkFlows.size(); ++l) {
			if (Link[l].Obs_volume[1] > 1) {
				totalLinkDev += linkFlows[l] - Link[l].Obs_volume[1];
				linkCount++;
			}
		}
		double avgLinkDev = (linkCount > 0 ? totalLinkDev / linkCount : 0.0);

		// VMT deviation: compute total VMT and its deviation from target
		double VMT = 0.0;
		for (size_t l = 0; l < linkFlows.size(); ++l) {
			VMT += linkFlows[l] * Link[l].length;
		}
		double vmtDev = (VMT_target > 0 ? (VMT - VMT_target) : 0.0);

		cout << "iter = " << iter << " [Deviation Log] OD avg deviation = " << avgODDev
			<< ", Link avg deviation = " << avgLinkDev
			<< ", VMT deviation = " << vmtDev << " ";

		logFile << "iter = " << iter << " [Deviation Log] OD avg deviation = " << avgODDev
			<< ", Link avg deviation = " << avgLinkDev
			<< ", VMT deviation = " << vmtDev << "\n";



		double sumGradSq = 0.0;
		// --- Compute gradients for each OD pair ---
		for (int m = 1; m <= numModes; ++m) {
			for (int Orig = 1; Orig <= numZones; ++Orig) {
				for (int Dest = 1; Dest <= numZones; ++Dest) {
					if (Orig == Dest)
						continue;


					float od_flow = MDODflow[m][Orig][Dest];
					if (od_flow <= 0)
						continue;

					gradient_MDODflow[m][Orig][Dest] = 0;

					double grad_link = 0.0;
					double grad_vmt = 0.0;
					// For link contributions, count usage of each link for this OD pair.
					int routeCount = 0;
					std::unordered_map<int, float> linkUsage;
					for (int route_id = 0; route_id < static_cast<int>(linkIndices[m][Orig][Dest].size()); ++route_id) {
						if (linkIndices[m][Orig][Dest][route_id].empty())
							continue;
						routeCount++;
						for (int link_id : linkIndices[m][Orig][Dest][route_id]) {

							if (Link[link_id].Obs_volume[1] > 1) {
								linkUsage[link_id] += 1;
							}
						}
					}
					if (routeCount == 0)
						continue;

					// In this example the link and VMT gradient terms are approximated.
					// (Depending on your formulation, you might want to recompute the sensitivities using the updated link flows.)
					for (auto& entry : linkUsage) {
						int link_id = entry.first;
						int count_usage = entry.second;
						double sensitivity = static_cast<double>(count_usage) / routeCount;
						if (Link[link_id].Obs_volume[1] > 1) {
							grad_link += 2.0 * w_link * (linkFlows[link_id] - Link[link_id].Obs_volume[1]) * sensitivity;
						}
						if (VMT_target > 1) {
							grad_vmt += 2.0 * w_vmt * (Link[link_id].length * sensitivity);
						}
					}
					double grad_od = 0;
					
					if(targetMDODflow[m][Orig][Dest]>=0.0)
						grad_od = 2.0 * w_od * (od_flow - targetMDODflow[m][Orig][Dest]);

					double total_grad = grad_link + grad_vmt + grad_od;
					gradient_MDODflow[m][Orig][Dest] = total_grad;
					sumGradSq += total_grad * total_grad;
				}
			}
		}

		// Check for convergence based on the norm of the gradient.
		if (std::sqrt(sumGradSq) < tol) {
			logFile << "Convergence reached after " << iter << " iterations.\n";
			std::cout << "Convergence reached after " << iter << " iterations.\n";
			break;
		}

		// --- Backtracking Line Search ---
		double alpha = step_size;
		// Save a copy of the current MDODflow.
		for (int m = 1; m <= numModes; ++m)
			for (int Orig = 1; Orig <= numZones; ++Orig)
				for (int Dest = 1; Dest <= numZones; ++Dest)
				{
					old_MDODflow[m][Orig][Dest] = MDODflow[m][Orig][Dest];
					candidate_MDODflow[m][Orig][Dest] = MDODflow[m][Orig][Dest];

				}
		// Prepare a candidate OD matrix.

		int line_search_iteration = 0; 

		while (true) {

		//	cout << "Line search iteration " << line_search_iteration << ": Step size chosen = " << alpha << endl; 
			line_search_iteration++; 
			// Update candidate MDODflow: candidate = oldMDODflow - alpha * gradient.
			for (int m = 1; m <= numModes; ++m) 
				for (int Orig = 1; Orig <= numZones; ++Orig) 
					for (int Dest = 1; Dest <= numZones; ++Dest) 
					{
						if (Orig == Dest)
							continue;

						if (old_MDODflow[m][Orig][Dest] > 0.001)
						{
							candidate_MDODflow[m][Orig][Dest] = old_MDODflow[m][Orig][Dest] - alpha * gradient_MDODflow[m][Orig][Dest];

							double larger_value = fmax(seed_MDODflow[m][Orig][Dest], targetMDODflow[m][Orig][Dest]);

							double lesser_value = seed_MDODflow[m][Orig][Dest];
								if(targetMDODflow[m][Orig][Dest] >=0.0)
									lesser_value = fmin(seed_MDODflow[m][Orig][Dest], targetMDODflow[m][Orig][Dest]);

							if (candidate_MDODflow[m][Orig][Dest] < lesser_value *0.5)
								candidate_MDODflow[m][Orig][Dest] = lesser_value * 0.5; // ensure non-negativity and resonableness

							if (candidate_MDODflow[m][Orig][Dest] > larger_value* 1.5)
								candidate_MDODflow[m][Orig][Dest] = larger_value * 1.5; // ensure uppper bound resonableness

						}
					}
				




				double F_candidate = computeObjectiveForMDOD(candidate_MDODflow, r2);
				logFile << "[Line Search] alpha = " << alpha
					<< " | F_candidate = " << F_candidate
					<< " | F_current = " << F_current
					<< " | Threshold = " << (F_current - armijo_c * alpha * sumGradSq) << "\n";
				// Check the Armijo condition:
				if (F_candidate <= F_current - armijo_c * alpha * sumGradSq) {
					//cout << "[Line Search] Armijo condition met: F_candidate (" << F_candidate
					//	<< ") <= F_current (" << F_current << ") - "
					//	<< armijo_c << " * alpha (" << alpha << ") * sumGradSq (" << sumGradSq << ")\n";

					logFile << "[Line Search] Armijo condition met: F_candidate (" << F_candidate
						<< ") <= F_current (" << F_current << ") - "
						<< armijo_c << " * alpha (" << alpha << ") * sumGradSq (" << sumGradSq << ")\n";
					count_Armijo_condition_NOT_met = 0;
					break;
				}
				else {
					logFile << "[Line Search] Armijo condition NOT met. Reducing alpha from " << alpha;
					alpha *= 0.5; // reduce step size
					logFile << " to " << alpha << "\n";
					if (alpha < 1e-8) {  // minimal threshold to avoid endless looping
						count_Armijo_condition_NOT_met++;
						cout << "[Line Search] Alpha dropped below threshold (1e-8). Exiting line search loop.\n";
						logFile << "[Line Search] Alpha dropped below threshold (1e-8). Exiting line search loop.\n";						break;
					}
				}
			
			logFile << "Iteration " << iter << ": Step size chosen = " << alpha
				<< ", Objective: " << F_current << " -> " << computeObjectiveForMDOD(candidate_MDODflow, r2) << "\n";
			cout << "Iteration " << iter << ": Step size chosen = " << alpha
				<< ", Objective: " << F_current << " -> " << computeObjectiveForMDOD(candidate_MDODflow, r2) << "\n";
			// Update MDODflow with the candidate from the line search.

			if (count_Armijo_condition_NOT_met >= 3)
			{
				cout << "count_Armijo_condition_NOT_met >= 3. Reaching final estimate\n";
				break;
			}
		}
			for (int m = 1; m <= numModes; ++m)
				for (int Orig = 1; Orig <= numZones; ++Orig)
					for (int Dest = 1; Dest <= numZones; ++Dest)
					{
						if (Orig == Dest)
							continue;
						else
						{
							MDODflow[m][Orig][Dest] = candidate_MDODflow[m][Orig][Dest];
						}
					}

			if (iter >= 5)
			{
				if (F_current > F_current_best + 1 || F_current > F_current_best * 1.1)
				{

					logFile << "Convergence reached as F_current > F_current_best\n";
					std::cout << "Convergence reached as F_current > F_current_best\n";
					break;
				}
			}
			// end of gradient descent loop

		
	}
    // --------------------------------------------------
    // Final synchronization of path flow and link volume 
    std::vector<double> linkFlows(number_of_links + 1, 0.0);
    std::vector<std::vector<double>> mode_linkFlows(numModes + 1, std::vector<double>(number_of_links + 1, 0.0));
    for (int m = 1; m <= numModes; ++m) {
        for (int Orig = 1; Orig <= numZones; ++Orig) {
            for (int Dest = 1; Dest <= numZones; ++Dest) {
                if (Orig == Dest)
                    continue;
                float od_flow = MDODflow[m][Orig][Dest];
                if (od_flow <= 0)
                    continue;
                int nRoutes = std::min(theta.size(), linkIndices[m][Orig][Dest].size());
                for (int route_id = 0; route_id < nRoutes; route_id++) {
                    float routeFlow = od_flow * theta[route_id];
                    if (linkIndices[m][Orig][Dest][route_id].empty())
                        continue;
                    for (int link_id : linkIndices[m][Orig][Dest][route_id]) {
                        if (link_id >= 0 && link_id < static_cast<int>(linkFlows.size())) {
                            linkFlows[link_id] += routeFlow;
                            mode_linkFlows[m][link_id] += routeFlow;
                        }
                    }
                }
            }
        }
        for (size_t l = 0; l < linkFlows.size(); ++l) 
		{
            MainVolume[l] = linkFlows[l];
            Link[l].mode_MainVolume[m] = mode_linkFlows[m][l];
        }



		// Start the R² calculation process for link flows.
		logFile << "Starting R² calculation for link flows.\n";

		// ---------------------------------------------------------------------
		// First pass: compute mean of observed volumes (for valid links)
		// ---------------------------------------------------------------------
		double sumObs = 0.0;
		int validCount = 0;
		for (size_t l = 1; l < linkFlows.size(); ++l) {
			if (Link[l].Obs_volume[1] > 1) {
				logFile << "Link " << l << " is valid. Observed volume: " << Link[l].Obs_volume[1] << "\n";
				sumObs += Link[l].Obs_volume[1];
				validCount++;
			}
			else {
				logFile << "Link " << l << " is invalid. Observed volume: " << Link[l].Obs_volume[1] << "\n";
			}
		}
		double meanObs = (validCount > 0) ? (sumObs / validCount) : 0.0;
		logFile << "Total valid links: " << validCount
			<< ", Sum of observed volumes: " << sumObs
			<< ", Mean observed volume: " << meanObs << "\n";

		// ---------------------------------------------------------------------
		// Second pass: Compute SSE and SStot for regression R² and accumulate
		// values for Pearson correlation computation.
		// ---------------------------------------------------------------------
		double SSE = 0.0;
		double SStot = 0.0;

		// Variables for Pearson correlation
		double sumObs_corr = 0.0;
		double sumPred_corr = 0.0;
		double sumObsPred = 0.0;
		double sumObsSq = 0.0;
		double sumPredSq = 0.0;
		int count = 0;

		for (size_t l = 1; l < linkFlows.size(); ++l) {
			if (Link[l].Obs_volume[1] > 1) {
				double obs = Link[l].Obs_volume[1];
				double pred = linkFlows[l];
				double diff = pred - obs;
				double squaredDiff = diff * diff;
				SSE += squaredDiff;
				double diffMean = obs - meanObs;
				double squaredDiffMean = diffMean * diffMean;
				SStot += squaredDiffMean;

				// Accumulate for correlation computation.
				count++;
				sumObs_corr += obs;
				sumPred_corr += pred;
				sumObsPred += obs * pred;
				sumObsSq += obs * obs;
				sumPredSq += pred * pred;

				logFile << "Link " << l << ": Predicted volume: " << pred
					<< ", Observed volume: " << obs
					<< ", Difference: " << diff
					<< ", Squared diff: " << squaredDiff
					<< ", Squared diff from mean: " << squaredDiffMean << "\n";
			}
		}
		logFile << "Total SSE (link): " << SSE << ", Total SStot: " << SStot << "\n";

		// ---------------------------------------------------------------------
		// Compute R² (regression) = 1 - SSE/SStot
		// ---------------------------------------------------------------------
		double r2_regression = 1.0;
		if (SStot > 0) {
			r2_regression = 1.0 - SSE / SStot;
			logFile << "Computed R2 (regression): " << r2_regression << " (1 - SSE/SStot).\n";
		}
		else {
			logFile << "SStot is 0. Data is constant or perfect. Set R2 (regression) to 1.0.\n";
		}

		// ---------------------------------------------------------------------
		// Compute Pearson correlation coefficient and then R2 (from correlation)
		// ---------------------------------------------------------------------
		double numerator = count * sumObsPred - sumObs_corr * sumPred_corr;
		double denominator_term1 = count * sumObsSq - sumObs_corr * sumObs_corr;
		double denominator_term2 = count * sumPredSq - sumPred_corr * sumPred_corr;
		double r_corr = 0.0;
		if (denominator_term1 > 0 && denominator_term2 > 0) {
			double denominator = std::sqrt(denominator_term1 * denominator_term2);
			if (denominator != 0) {
				r_corr = numerator / denominator;
			}
		}
		double r2_correlation = r_corr * r_corr;
		logFile << "Computed Pearson correlation: " << r_corr
			<< ", R2 (from correlation): " << r2_correlation << "\n";

		logFile << "ODME process finished.\n";

		// ---------------------------------------------------------------------
		// Output the results
		// ---------------------------------------------------------------------
		std::cout << "R2 (regression) = " << r2_regression << std::endl;
		std::cout << "R2 (from correlation) = " << r2_correlation << std::endl;

		logFile << "ODME process finished.\n";
    }


    logFile << "ODME process finished.\n";

    logFile.close();
}

// Define a structure to hold all relevant route data.
struct RouteData {
	int firstRouteID;                // The first encountered route_id (optional)
	int unique_route_id;             // A sequential unique id for output
	std::string nodeIDsStr;          // Node IDs (string)
	std::string linkIDsStr;          // Link IDs (string)
	std::string linkLengthStr;          // Link Length (string)
	double totalDistance;
	double totalFreeFlowTravelTime;
	double totalTravelTime;
	double accumulatedTheta = 0;         // Sum of theta for all routes with this key
	std::string routeKey;            // Unique key (e.g., based on node and link sums)
};

void OutputRouteDetails(const std::string& filename, std::vector<double> theta)
{
	int agent_id = 1; 
	std::ofstream outputFile(filename);  // Open the file for writing

	if (linkIndices.size() == 0)
		return; 
	// Write the CSV header in lowercase
	outputFile << "mode,route_id,o_zone_id,d_zone_id,unique_route_id,prob,node_ids,link_ids,distance_mile,total_distance_km,total_free_flow_travel_time,total_travel_time,route_key,seed_od_volume,target_od_volume,final_est_od_volume,volume,";

	outputFile << "\n";

	std::ofstream outputFile_vehicle("vehicle.csv");  // Open the file for writing

	outputFile_vehicle << "agent_id,departure_time,departure_time_hhmmss,mode,route_id,o_zone_id,d_zone_id,unique_route_id,node_ids,link_ids,total_distance_mile,total_distance_km,total_free_flow_travel_time,total_travel_time,route_volume,\n";




	for (int m = 1; m < linkIndices.size(); ++m)
	{
		for (int Orig = 1; Orig < linkIndices[m].size(); ++Orig)
		{
			for (int Dest = 1; Dest < linkIndices[m][Orig].size(); ++Dest)
			{
				std::unordered_map<std::string, RouteData> uniqueRoutes;

				// Reset unique route counter.
				int unique_route_id = 1;

				// Loop over all candidate routes.
				for (int route_id = 0; route_id < linkIndices[m][Orig][Dest].size(); ++route_id)
				{
					if (!linkIndices[m][Orig][Dest][route_id].empty())
					{
						double totalDistance = 0.0;
						double totalFreeFlowTravelTime = 0.0;
						double totalTravelTime = 0.0;
						std::string nodeIDsStr;
						std::string linkIDsStr;
						std::string linkLengthStr;
						
						int nodeSum = 0;  // Sum of node IDs (for uniqueness key)
						int linkSum = 0;  // Sum of link IDs (for uniqueness key)

						// Process the route in reverse order (as in your code).
						for (int i = linkIndices[m][Orig][Dest][route_id].size() - 1; i >= 0; --i)
						{
							int k = linkIndices[m][Orig][Dest][route_id][i];

							// Append the from_node_id and accumulate its value.
							int fromNodeID = Link[k].external_from_node_id;
							nodeIDsStr += std::to_string(fromNodeID) + ";";
							nodeSum += fromNodeID;

							// Append the link index (as a proxy for link ID) and accumulate.
							linkIDsStr += std::to_string(k) + ";";
							//double length = Link[k].length; 
							//linkLengthStr += std::to_string(length) + ";";
							linkSum += k;

							// Sum the distance and travel times.
							totalDistance += Link[k].length;
							totalFreeFlowTravelTime += Link[k].FreeTravelTime;
							totalTravelTime += Link[k].Travel_time;

							// For the first link in the (reversed) order, also add the to_node_id.
							if (i == 0)
							{
								int toNodeID = Link[k].external_to_node_id;
								nodeIDsStr += std::to_string(toNodeID);
								nodeSum += toNodeID;
							}
						}

						// Create a unique key for the route.
						std::string routeKey = std::to_string(nodeSum) + "_" + std::to_string(linkSum);

						// Check if this unique route already exists.
						auto it = uniqueRoutes.find(routeKey);
						if (it == uniqueRoutes.end())
						{
							// Not found: create a new RouteData entry.
							RouteData rd;
							rd.firstRouteID = route_id;      // Save the first encountered candidate's route_id.
							rd.unique_route_id = unique_route_id;  // Unique id for output.
							rd.nodeIDsStr = nodeIDsStr;
							rd.linkIDsStr = linkIDsStr;
							//rd.linkLengthStr = linkLengthStr; 
							rd.totalDistance = totalDistance;
							rd.totalFreeFlowTravelTime = totalFreeFlowTravelTime;
							rd.totalTravelTime = totalTravelTime;

							if(route_id < theta.size() )
							{
							rd.accumulatedTheta = theta[route_id];  // Initialize with current theta.
							}
							else
							{
								rd.accumulatedTheta = unique_route_id *1.0/uniqueRoutes.size();

							}
							rd.routeKey = routeKey;

							// Insert the new unique route.
							uniqueRoutes[routeKey] = rd;
							unique_route_id++;
						}
						else
						{
							// Duplicate route: accumulate theta.
							it->second.accumulatedTheta += theta[route_id];
							// Optionally, you could verify that other route details (e.g., distances) match.
						}
					}
				}

				// Now output the unique routes with their accumulated theta values.
				for (const auto& pair : uniqueRoutes)
				{
					const RouteData& rd = pair.second;
					float od_volume = 1;
					float seed_od_volume = 1;
					float target_od_volume = 0;

					float route_volume = 1;

					double accumulatedTheta = rd.accumulatedTheta;
					if (TotalAssignIterations <= 1)
						accumulatedTheta = 1.0;
					int org_origin_zone = g_map_node_seq_no_2_external_node_id[Orig];
					int org_dest_zone = g_map_node_seq_no_2_external_node_id[Dest];

					if(MDODflow!=NULL)
					{


						seed_od_volume = seed_MDODflow[m][org_origin_zone][org_dest_zone];
						od_volume = MDODflow[m][org_origin_zone][org_dest_zone];
						target_od_volume = targetMDODflow[m][org_origin_zone][org_dest_zone];
						route_volume = od_volume * accumulatedTheta;
					} 



					if(no_zones <3000 || (no_zones>=3000 && od_volume >=1.0))
					{
					// (Optional) Remove trailing semicolon from linkIDsStr if needed.
					std::string cleanedLinkIDsStr = rd.linkIDsStr;
					if (!cleanedLinkIDsStr.empty() && cleanedLinkIDsStr.back() == ';')
						cleanedLinkIDsStr.pop_back();

					outputFile << g_mode_type_vector[m].mode_type.c_str() << ","
						<< rd.firstRouteID << ","  // or route_id (the first candidate id)
						<< org_origin_zone << ","
						<< org_dest_zone << ","
						<< rd.unique_route_id << ","
						<< accumulatedTheta << ","
						<< rd.nodeIDsStr << ","
						<< cleanedLinkIDsStr << ","
						//<< rd.linkLengthStr << ","
						<< rd.totalDistance << ","
						<< rd.totalDistance * 1.609 << ","
						<< rd.totalFreeFlowTravelTime << ","
						<< rd.totalTravelTime << ","
						<< rd.routeKey << ","
						<< seed_od_volume << "," 
						<< target_od_volume << "," 
						<<  od_volume << ","

						<< route_volume << "\n";

					//vehicle.csv output block
					if (vehicle_log_flag ==1 &&  !linkIndices[m][Orig][Dest][rd.firstRouteID].empty())
					{


						std::vector<double> linkTravelTimes; // Store travel times for each link
						// Record the travel time for this link
						for (int i = linkIndices[m][Orig][Dest][rd.firstRouteID].size() - 1; i >= 0; --i)
						{
							int k = linkIndices[m][Orig][Dest][rd.firstRouteID][i];
							linkTravelTimes.push_back(Link[k].Travel_time);
						}



						int integer_volume = static_cast<int>(route_volume);
						float residual = route_volume - integer_volume;

						// Add residual probability
						if (generate_random() < residual) {
							++integer_volume;
						}
						int total_time_window_min = (demand_period_ending_hours - demand_period_starting_hours) * 60;

						float departure_time = demand_period_starting_hours * 60;


						if (integer_volume > 0)
						{


							float route_time_increment = static_cast<float>(total_time_window_min) / integer_volume;


							for (int v = 0; v < integer_volume; ++v)
							{
								float time_increment_in_min = route_time_increment;
								if (integer_volume > 10)
								{
									departure_time = demand_period_starting_hours * 60 + v * route_time_increment;
									time_increment_in_min = route_time_increment;
								}
								else
								{
									departure_time = demand_period_starting_hours * 60 + agent_id % total_time_window_min;
									time_increment_in_min = 1;
								}

								std::vector<double> nodeArrivalTimes; // Store arrival times for each node
								double cumulativeTime = departure_time;
								// Calculate arrival times for each node
								for (double travelTime : linkTravelTimes) {
									cumulativeTime += travelTime;
									nodeArrivalTimes.push_back(cumulativeTime);
								}

								std::string timeSequenceStr;
								// Convert arrival times to a semicolon-separated string
								timeSequenceStr.clear();
								for (double time : nodeArrivalTimes) {
									timeSequenceStr += convert_minutes_to_hhmmss(time) + ";";
								}
								if (!timeSequenceStr.empty()) timeSequenceStr.pop_back();

								//departure_time += std::rand() * time_increment_in_min;

								std::string departure_time_hhmmss = convert_minutes_to_hhmmss(departure_time);

								// Write the data for this OD pair and route to the CSV file
								outputFile_vehicle << agent_id << "," << departure_time << ","
									<< departure_time_hhmmss << ","
									<< g_mode_type_vector[m].mode_type.c_str() << ","
									<< rd.firstRouteID << "," << org_origin_zone << "," << org_dest_zone << "," << unique_route_id << ","
									<< rd.nodeIDsStr << "," << cleanedLinkIDsStr << ","
									<< rd.totalDistance << "," << rd.totalDistance * 1.609 << "," << rd.totalFreeFlowTravelTime << ","
									<< rd.totalTravelTime << "," << route_volume << "\n";



								agent_id++;
							}

							unique_route_id++;
						}
						//else
						//{
						//    // Duplicate path found, skipping output
						//    //std::cout << "Duplicate route skipped for Origin: " << Orig << ", Destination: " << Dest << "\n";
						//}
					}
					}
					else
					{

						// Process the route in reverse order (as in your code).
						for (int i = linkIndices[m][Orig][Dest][rd.firstRouteID].size() - 1; i >= 0; --i)
						{
							int k = linkIndices[m][Orig][Dest][rd.firstRouteID][i];
							Link[k].background_volume += route_volume;
						}
					}
				}

			}
		}
	}

	// Close the file after writing
	outputFile.close();

	outputFile_vehicle.close();
	std::cout << "Output written to " << filename << std::endl;

}





void OutputODPerformance(const std::string& filename)
{
	std::ofstream outputFile(filename);
	std::ofstream googleMapsFile("google_maps_od_distance.csv");

	if (!googleMapsFile.is_open())
	{
		std::cerr << "Error: Could not open Google Maps OD distance file." << std::endl;
		return;
	}

	googleMapsFile << "route_key,mode,o_zone_id,d_zone_id,volume,total_distance_mile,total_distance_km,straight_line_distance_mile,straight_line_distance_km,distance_ratio,total_free_flow_travel_time,total_congestion_travel_time,google_maps_http_link,WKT_geometry\n";

	outputFile << "route_key,mode,o_zone_id,d_zone_id,o_x_coord,o_y_coord,d_x_coord,d_y_coord,total_distance_mile,total_distance_km,straight_line_distance_mile,straight_line_distance_km,distance_ratio,total_free_flow_travel_time,total_congestion_travel_time,volume,WKT_geometry\n";

	double grand_totalDistance = 0.0;
	double grand_totalFreeFlowTravelTime = 0.0;
	double grand_totalTravelTime = 0.0;
	double grand_total_count = 0;
	double grand_total_straight_line_distance = 0.0;
	double grand_total_distance_ratio = 0.0;

	for (int m = 1; m < linkIndices.size(); ++m)
	{
		for (int Orig = 1; Orig < linkIndices[m].size(); ++Orig)
		{
			for (int Dest = 1; Dest < linkIndices[m][Orig].size(); ++Dest)
			{
				std::unordered_map<std::string, bool> uniqueRoutes;
				for (int route_id = 0; route_id < linkIndices[m][Orig][Dest].size(); ++route_id)
				{
					if (!linkIndices[m][Orig][Dest][route_id].empty())
					{
						double totalDistance = 0.0;
						double totalFreeFlowTravelTime = 0.0;
						double totalTravelTime = 0.0;

						std::string nodeIDsStr;
						std::string linkIDsStr;

						long nodeSum = 0;
						long linkSum = 0;


						for (int i = linkIndices[m][Orig][Dest][route_id].size() - 1; i >= 0; --i)
						{
							long k = linkIndices[m][Orig][Dest][route_id][i];

							int fromNodeID = Link[k].external_from_node_id;
							int internal_fromNodeID = Link[k].internal_from_node_id;
							double from_x = g_node_vector[internal_fromNodeID].x;
							double from_y = g_node_vector[internal_fromNodeID].y;

							nodeIDsStr += std::to_string(fromNodeID) + ";";
							nodeSum += fromNodeID;

							linkIDsStr += std::to_string(k) + ";";
							linkSum += k;

							totalDistance += Link[k].length;
							totalFreeFlowTravelTime += Link[k].FreeTravelTime;
							totalTravelTime += Link[k].Travel_time;


							if (i == 0)
							{
								int toNodeID = Link[k].external_to_node_id;
								int internal_toNodeID = Link[k].internal_to_node_id;
								double to_x = g_node_vector[internal_toNodeID].x;
								double to_y = g_node_vector[internal_toNodeID].y;

								nodeIDsStr += std::to_string(toNodeID);
								nodeSum += toNodeID;
							}
						}

						std::string WKT_geometry = "LINESTRING(";
						if (no_zones < 300 || (no_zones >= 300 && MDODflow[m][Orig][Dest] >= 10.0))
						{

							// Construct WKT format from node coordinates
							for (int i = linkIndices[m][Orig][Dest][route_id].size() - 1; i >= 0; --i)
							{
								long k = linkIndices[m][Orig][Dest][route_id][i];

								int fromNodeID = Link[k].external_from_node_id;
								int internal_fromNodeID = Link[k].internal_from_node_id;
								double from_x = g_node_vector[internal_fromNodeID].x;
								double from_y = g_node_vector[internal_fromNodeID].y;



								WKT_geometry += std::to_string(from_x) + " " + std::to_string(from_y) + ",";

								if (i == 0)
								{
									int toNodeID = Link[k].external_to_node_id;
									int internal_toNodeID = Link[k].internal_to_node_id;
									double to_x = g_node_vector[internal_toNodeID].x;
									double to_y = g_node_vector[internal_toNodeID].y;

									WKT_geometry += std::to_string(to_x) + " " + std::to_string(to_y);
								}
							}


						}
						WKT_geometry += ")"; // Close the LINESTRING

						std::string routeKey = std::to_string(Orig) + "_" + std::to_string(Dest) + "_" + g_mode_type_vector[m].mode_type;

						if (uniqueRoutes.find(routeKey) == uniqueRoutes.end())
						{
							uniqueRoutes[routeKey] = true;
							float volume = MDODflow[m][Orig][Dest];

							grand_totalDistance += totalDistance * volume;
							grand_totalFreeFlowTravelTime += totalFreeFlowTravelTime * volume;
							grand_totalTravelTime += totalTravelTime * volume;
							grand_total_count += volume;

							int internal_o_node_id = g_map_external_node_id_2_node_seq_no[Orig];
							int internal_d_node_id = g_map_external_node_id_2_node_seq_no[Dest];
							double o_x_coord = g_node_vector[internal_o_node_id].x;
							double o_y_coord = g_node_vector[internal_o_node_id].y;
							double d_x_coord = g_node_vector[internal_d_node_id].x;
							double d_y_coord = g_node_vector[internal_d_node_id].y;

							double straight_line_distance_mile = 3958.8 * 2 * atan2(sqrt(pow(sin((d_y_coord - o_y_coord) * M_PI / 360.0), 2) + cos(o_y_coord * M_PI / 180.0) * cos(d_y_coord * M_PI / 180.0) * pow(sin((d_x_coord - o_x_coord) * M_PI / 360.0), 2)), sqrt(1 - pow(sin((d_y_coord - o_y_coord) * M_PI / 360.0), 2) - cos(o_y_coord * M_PI / 180.0) * cos(d_y_coord * M_PI / 180.0) * pow(sin((d_x_coord - o_x_coord) * M_PI / 360.0), 2)));
							double straight_line_distance_km = straight_line_distance_mile * 1.609;

							double distance_ratio = (straight_line_distance_mile > 0.001) ? totalDistance / straight_line_distance_mile : 1.0;

							grand_total_straight_line_distance += straight_line_distance_mile * volume;
							grand_total_distance_ratio += distance_ratio * volume;

							outputFile << routeKey << "," << g_mode_type_vector[m].mode_type << "," << Orig << "," << Dest << ","
								<< o_x_coord << "," << o_y_coord << "," << d_x_coord << "," << d_y_coord << ","
								<< totalDistance << "," << totalDistance * 1.609 << "," << straight_line_distance_mile << "," << straight_line_distance_km << ","
								<< distance_ratio << "," << totalFreeFlowTravelTime << "," << totalTravelTime << "," << volume << ",\"" << WKT_geometry << "\"\n";


							if (volume >= 1.0 && no_zones <= 3000 || volume >= 10.0 && no_zones > 3000)
							{
								std::string googleMapsLink = "https://www.google.com/maps/dir/" + std::to_string(o_y_coord) + "," + std::to_string(o_x_coord) + "/" + std::to_string(d_y_coord) + "," + std::to_string(d_x_coord) + "/";

								googleMapsFile << routeKey << "," << g_mode_type_vector[m].mode_type << "," << Orig << "," << Dest << "," << volume << ","
									<< totalDistance << "," << totalDistance * 1.609 << "," << straight_line_distance_mile << "," << straight_line_distance_km << ","
									<< distance_ratio << "," << totalFreeFlowTravelTime << "," << totalTravelTime << ",\"" << googleMapsLink << "\",\"" << WKT_geometry << "\"\n";
							}
						}
					}
				}
			}
		}
	}
	outputFile.close();
	googleMapsFile.close();
	std::cout << "Output written to " << filename << std::endl;
	std::cout << "Google Maps links saved to google_maps_od_distance.csv" << std::endl;
}

// ===== Data Structures for Aggregation =====

// Aggregation at the mode (system) level.
struct AggregationData {
	double total_volume = 0.0;
	double total_VMT = 0.0;      // VMT in miles * volume
	double total_VHT = 0.0;      // VHT in hours * volume
	double total_FF_VHT = 0.0;   // Free-flow VHT in hours * volume
	double total_delay = 0.0;    // (congested - free-flow time) in hours * volume
	int count_OD = 0;
};

// Aggregation for accessibility at origins.
struct OriginStats {
	double total_volume = 0.0;
	double total_distance = 0.0;     // Sum(distance * volume) in miles
	double total_travel_time = 0.0;  // Sum(congested travel time * volume) in minutes
	int count_dest = 0;              // Number of destination flows (nonzero OD pairs)
};

// Aggregation for accessibility at destinations.
struct DestinationStats {
	double total_volume = 0.0;
	double total_distance = 0.0;
	double total_travel_time = 0.0;
	int count_orig = 0;              // Number of origin flows (nonzero OD pairs)
};

// ===== New Function for Aggregated Performance and Accessibility =====
void GenerateAggregatedPerformanceAndAccessibility()
{
	// Mode-level aggregator.
	std::map<std::string, AggregationData> modeAgg;

	// Aggregators for origin and destination accessibility.
	std::vector<OriginStats> originStats(no_zones + 1);         // 1-indexed zones.
	std::vector<DestinationStats> destinationStats(no_zones + 1);

	// Process accessible OD pairs.
	// (Assumes linkIndices and MDODflow are 1-indexed for modes and zones.)
	for (int m = 1; m < linkIndices.size(); ++m)
	{
		std::string modeStr = g_mode_type_vector[m].mode_type;
		for (int Orig = 1; Orig < linkIndices[m].size(); ++Orig)
		{
			for (int Dest = 1; Dest < linkIndices[m][Orig].size(); ++Dest)
			{
				// Process only the first non-empty route per OD pair.
				for (int route_id = 0; route_id < linkIndices[m][Orig][Dest].size(); ++route_id)
				{
					if (linkIndices[m][Orig][Dest][route_id].empty())
						continue;

					double totalDistance = 0.0;           // in miles
					double totalFreeFlowTravelTime = 0.0;   // in minutes
					double totalTravelTime = 0.0;           // in minutes
					for (int i = linkIndices[m][Orig][Dest][route_id].size() - 1; i >= 0; --i)
					{
						long k = linkIndices[m][Orig][Dest][route_id][i];
						totalDistance += Link[k].length;
						totalFreeFlowTravelTime += Link[k].FreeTravelTime;
						totalTravelTime += Link[k].Travel_time;
					}

					float volume = MDODflow[m][Orig][Dest];
					if (volume <= 0)
						break;  // No flow for this OD pair; skip further processing.

					// Update mode-level aggregator.
					AggregationData& agg = modeAgg[modeStr];
					agg.total_volume += volume;
					agg.total_VMT += totalDistance * volume;
					// Convert travel times from minutes to hours.
					agg.total_VHT += (totalTravelTime / 60.0) * volume;
					agg.total_FF_VHT += (totalFreeFlowTravelTime / 60.0) * volume;
					agg.total_delay += ((totalTravelTime - totalFreeFlowTravelTime) / 60.0) * volume;
					agg.count_OD += 1;

					// Update origin accessibility aggregator.
					originStats[Orig].total_volume += volume;
					originStats[Orig].total_distance += totalDistance * volume;
					originStats[Orig].total_travel_time += totalTravelTime * volume;
					originStats[Orig].count_dest += 1;

					// Update destination accessibility aggregator.
					destinationStats[Dest].total_volume += volume;
					destinationStats[Dest].total_distance += totalDistance * volume;
					destinationStats[Dest].total_travel_time += totalTravelTime * volume;
					destinationStats[Dest].count_orig += 1;

					// Process only the first non-empty route for this OD pair.
					break;
				}
			}
		}
	}

	// ----- Write system_performance.csv -----
	std::ofstream systemPerfFile("system_performance.csv");
	if (!systemPerfFile.is_open())
	{
		std::cerr << "Error: Could not open system_performance.csv for writing." << std::endl;
		return;
	}
	// Header with extra columns.
	systemPerfFile << "mode_type,total_volume,PMT (VMT in miles),PHT (VHT in hours),Delay (hours),TTI,"
		<< "avg_distance_mile,avg_distance_km,avg_travel_time_min,avg_travel_delay_in_min,"
		<< "avg_speed_mph,avg_speed_kmph\n";
	for (const auto& kv : modeAgg)
	{
		const std::string& mode = kv.first;
		const AggregationData& data = kv.second;
		double TTI = (data.total_FF_VHT > 0) ? (data.total_VHT / data.total_FF_VHT) : 1.0;
		double avg_distance_mile = (data.total_volume > 0) ? (data.total_VMT / data.total_volume) : 0.0;
		double avg_distance_km = avg_distance_mile * 1.609;
		double avg_travel_time_min = (data.total_volume > 0) ? ((data.total_VHT / data.total_volume) * 60.0) : 0.0;
		double avg_travel_delay_min = (data.total_volume > 0) ? ((data.total_delay / data.total_volume) * 60.0) : 0.0;
		double avg_speed_mph = (avg_travel_time_min > 0) ? (avg_distance_mile / (avg_travel_time_min / 60.0)) : 0.0;
		double avg_speed_kmph = (avg_travel_time_min > 0) ? (avg_distance_km / (avg_travel_time_min / 60.0)) : 0.0;

		systemPerfFile << mode << ","
			<< data.total_volume << ","
			<< data.total_VMT << ","
			<< data.total_VHT << ","
			<< data.total_delay << ","
			<< TTI << ","
			<< avg_distance_mile << ","
			<< avg_distance_km << ","
			<< avg_travel_time_min << ","
			<< avg_travel_delay_min << ","
			<< avg_speed_mph << ","
			<< avg_speed_kmph << "\n";
	}
	systemPerfFile.close();

	// ----- Write origin_accessibility.csv -----
	// This file lists every origin zone (from 1 to no_zones) and includes a Google Maps search link.
	std::ofstream originFile("origin_accessibility.csv");
	if (!originFile.is_open())
	{
		std::cerr << "Error: Could not open origin_accessibility.csv for writing." << std::endl;
		return;
	}
	originFile << "origin_zone_id,total_volume,number_of_destinations,avg_distance_mile,avg_travel_time_min,google_maps_link\n";
	for (int o = 1; o <= no_zones; ++o)
	{
		double vol = originStats[o].total_volume;
		double avg_distance = (vol > 0) ? (originStats[o].total_distance / vol) : 0.0;
		double avg_travel_time = (vol > 0) ? (originStats[o].total_travel_time / vol) : 0.0;
		std::string googleLink = "";
		if (g_map_external_node_id_2_node_seq_no[o] < g_node_vector.size())
		{
			int node_index = g_map_external_node_id_2_node_seq_no[o];
			double x = g_node_vector[node_index].x;
			double y = g_node_vector[node_index].y;
			googleLink = "https://www.google.com/maps/search/?api=1&query=" + std::to_string(y) + "," + std::to_string(x);
		}
		// Enclose the google link in double quotes.
		originFile << o << ","
			<< vol << ","
			<< originStats[o].count_dest << ","
			<< avg_distance << ","
			<< avg_travel_time << ","
			<< "\"" << googleLink << "\"" << "\n";
	}
	originFile.close();

	// ----- Write destination_accessibility.csv -----
	// This file lists every destination zone (from 1 to no_zones) with a Google Maps search link.
	std::ofstream destFile("destination_accessibility.csv");
	if (!destFile.is_open())
	{
		std::cerr << "Error: Could not open destination_accessibility.csv for writing." << std::endl;
		return;
	}
	destFile << "destination_zone_id,total_volume,number_of_origins,avg_distance_mile,avg_travel_time_min,google_maps_link\n";
	for (int d = 1; d <= no_zones; ++d)
	{
		double vol = destinationStats[d].total_volume;
		double avg_distance = (vol > 0) ? (destinationStats[d].total_distance / vol) : 0.0;
		double avg_travel_time = (vol > 0) ? (destinationStats[d].total_travel_time / vol) : 0.0;
		std::string googleLink = "";
		if (g_map_external_node_id_2_node_seq_no[d] < g_node_vector.size())
		{
			int node_index = g_map_external_node_id_2_node_seq_no[d];
			double x = g_node_vector[node_index].x;
			double y = g_node_vector[node_index].y;
			googleLink = "https://www.google.com/maps/search/?api=1&query=" + std::to_string(y) + "," + std::to_string(x);
		}
		destFile << d << ","
			<< vol << ","
			<< destinationStats[d].count_orig << ","
			<< avg_distance << ","
			<< avg_travel_time << ","
			<< "\"" << googleLink << "\"" << "\n";
	}
	destFile.close();

	// ----- Write inaccessible_od.csv -----
	// For each mode, origin, and destination, check for a non-empty route.
	// If no non-empty route exists and the OD flow (volume) is > 0, then output this OD pair.
	std::ofstream inacFile("inaccessible_od.csv");
	if (!inacFile.is_open())
	{
		std::cerr << "Error: Could not open inaccessible_od.csv for writing." << std::endl;
		return;
	}
	inacFile << "mode_type,origin_zone_id,destination_zone_id,google_maps_http_link\n";
	for (int m = 1; m < linkIndices.size(); ++m)
	{
		std::string modeStr = g_mode_type_vector[m].mode_type;
		for (int Orig = 1; Orig < linkIndices[m].size(); ++Orig)
		{
			for (int Dest = 1; Dest < linkIndices[m][Orig].size(); ++Dest)
			{
				// Check if any route exists for this OD.
				bool routeExists = false;
				for (int route_id = 0; route_id < linkIndices[m][Orig][Dest].size(); ++route_id)
				{
					if (!linkIndices[m][Orig][Dest][route_id].empty())
					{
						routeExists = true;
						break;
					}
				}
				// If no route exists but the flow (volume) is > 0, then mark as inaccessible.
				if (!routeExists && MDODflow[m][Orig][Dest] > 0)
				{
					std::string googleLink = "";
					if (Orig < g_map_external_node_id_2_node_seq_no.size() &&
						Dest < g_map_external_node_id_2_node_seq_no.size() &&
						g_map_external_node_id_2_node_seq_no[Orig] < g_node_vector.size() &&
						g_map_external_node_id_2_node_seq_no[Dest] < g_node_vector.size())
					{
						int originNodeIndex = g_map_external_node_id_2_node_seq_no[Orig];
						int destNodeIndex = g_map_external_node_id_2_node_seq_no[Dest];
						double o_x = g_node_vector[originNodeIndex].x;
						double o_y = g_node_vector[originNodeIndex].y;
						double d_x = g_node_vector[destNodeIndex].x;
						double d_y = g_node_vector[destNodeIndex].y;
						// Construct a Google Maps directions link.
						googleLink = "https://www.google.com/maps/dir/" + std::to_string(o_y) + "," + std::to_string(o_x) + "/" +
							std::to_string(d_y) + "," + std::to_string(d_x) + "/";
					}

					int org_origin_zone = g_map_node_seq_no_2_external_node_id[Orig];
					int org_dest_zone = g_map_node_seq_no_2_external_node_id[Dest];

					inacFile << modeStr << ","
						<< org_origin_zone << ","
						<< org_dest_zone << ","
						<< "\"" << googleLink << "\"" << "\n";
				}
			}
		}
	}
	inacFile.close();

	std::cout << "System performance written to system_performance.csv" << std::endl;
	std::cout << "Origin accessibility written to origin_accessibility.csv" << std::endl;
	std::cout << "Destination accessibility written to destination_accessibility.csv" << std::endl;
	std::cout << "Inaccessible OD pairs written to inaccessible_od.csv" << std::endl;

}
int get_number_of_nodes_from_node_file(int& number_of_zones, int& l_FirstThruNode)
{
	number_of_zones = 0;
	CDTACSVParser parser_node;
	l_FirstThruNode = 1;
	int number_of_nodes = 0;

	if (parser_node.OpenCSVFile("node.csv", true))
	{
		while (parser_node.ReadRecord())  // if this line contains [] mark, then we will also read
			// field headers.
		{
			// Read node id
			int node_id = 0;
			int zone_id = 0;
			parser_node.GetValueByFieldName("node_id", node_id);
			parser_node.GetValueByFieldName("zone_id", zone_id);

			if (zone_id == 0 && node_id == 0)
			{
				printf("Error: node_id and zone_id should start from 1\n");
			}
			if (zone_id >= 1 && zone_id != node_id)
			{
				printf("Error: zone_id should be the same as node_id but zone_id  = %d, node_id = %d\n", zone_id, node_id);
			}

			if (g_map_external_node_id_2_node_seq_no.find(node_id) != g_map_external_node_id_2_node_seq_no.end())
			{
				printf("Error: duplicated node_id = %d\n", node_id);
				continue;

			}

			g_map_node_seq_no_2_external_node_id[number_of_nodes + 1] = node_id;



			g_map_external_node_id_2_node_seq_no[node_id] =
				number_of_nodes + 1;  // this code node sequential number starts from 1

			if (zone_id >= 1 && zone_id > number_of_zones)
				number_of_zones = zone_id;

			if(first_through_node_id_input == -1)  // auto identification
			{
			if (zone_id == 0 && l_FirstThruNode == 1 /* not initialized*/)
				l_FirstThruNode = number_of_nodes + 1;  //use sequential node id
			}

			if (g_tap_log_file == 1)
			{
				fprintf(logfile, "node_id = %d, node_seq_no = %d\n", node_id, g_map_external_node_id_2_node_seq_no[node_id]);

			}

			number_of_nodes++;
		}


		parser_node.CloseCSVFile();
	}

	g_node_vector.resize(number_of_nodes + 1);

	int internal_zone_no_count = 0;
	if (parser_node.OpenCSVFile("node.csv", true))
	{
		while (parser_node.ReadRecord())  // if this line contains [] mark, then we will also read
			// field headers.
		{
			// Read node id
			int node_id = 0;
			parser_node.GetValueByFieldName("node_id", node_id);


			int internal_node_id = g_map_external_node_id_2_node_seq_no[node_id];
			double x_coord, y_coord;
			parser_node.GetValueByFieldName("x_coord", x_coord);
			parser_node.GetValueByFieldName("y_coord", y_coord);
			int zone_id = -1;
			parser_node.GetValueByFieldName("zone_id", zone_id);
			
			g_node_vector[internal_node_id].x = x_coord;
			g_node_vector[internal_node_id].y = y_coord;
			g_node_vector[internal_node_id].node_id = node_id; 
			if(zone_id>=1)
			{ 
			g_node_vector[internal_node_id].zone_id = zone_id;

			g_node_vector[internal_node_id].internal_zone_no = internal_zone_no_count;
			g_map_internal_zone_no_2_node_seq_no[g_node_vector[internal_node_id].internal_zone_no] = internal_node_id;
			internal_zone_no_count++;
			}
		}

		parser_node.CloseCSVFile();
	}

	return number_of_nodes;
}

int get_number_of_links_from_link_file()
{
	CDTACSVParser parser_link;

	int number_of_links = 0;

	if (parser_link.OpenCSVFile("link.csv", true))
	{
		while (parser_link.ReadRecord())  // if this line contains [] mark, then we will also read
			// field headers.
		{
			int link_id = 0;
			int internal_from_node_id = 0;
			// Read link_id
			parser_link.GetValueByFieldName("link_id", link_id);
			parser_link.GetValueByFieldName("from_node_id", internal_from_node_id);

			number_of_links++;
		}

		parser_link.CloseCSVFile();
	}

	return number_of_links;
}

void createSettingsFile(const std::string& fileName) {
	std::ofstream file(fileName);
	if (!file.is_open()) {
		std::cerr << "Could not create the file: " << fileName << std::endl;
		return;
	}

	// Writing the headers of the CSV
	file << "number_of_iterations,number_of_processors,demand_period_starting_hours,demand_period_ending_hours,first_through_node_id,base_demand_mode,route_output,vehicle_output,simulation_output,log_file,odme_mode,odme_vmt\n";

	// Writing the sample data (from your provided file)
	file << "20,8,7,8,-1,0,1,0,0,0,0\n";

	file.close();
	std::cout << "sample_settings.csv file created successfully!" << std::endl;
}

void read_settings_file()
{

	createSettingsFile("sample_settings.csv");

	CDTACSVParser parser_settings;

	if (parser_settings.OpenCSVFile("settings.csv", true))
	{
		while (parser_settings.ReadRecord())  // if this line contains [] mark, then we will also read
			// field headers.
		{
			g_number_of_processors = 4;

			parser_settings.GetValueByFieldName("number_of_iterations", TotalAssignIterations);

			if (TotalAssignIterations == 0)
				g_accessibility_only_mode = 1;
			else
				g_accessibility_only_mode = 0; 

			parser_settings.GetValueByFieldName("number_of_processors", g_number_of_processors);
			parser_settings.GetValueByFieldName("demand_period_starting_hours", demand_period_starting_hours);
			parser_settings.GetValueByFieldName("demand_period_ending_hours", demand_period_ending_hours);
			parser_settings.GetValueByFieldName("first_through_node_id", first_through_node_id_input);

			parser_settings.GetValueByFieldName("log_file", g_tap_log_file);
			parser_settings.GetValueByFieldName("base_demand_mode", g_base_demand_mode);
			parser_settings.GetValueByFieldName("odme_mode", g_ODME_mode);
			parser_settings.GetValueByFieldName("odme_vmt", g_ODME_obs_VMT);
			parser_settings.GetValueByFieldName("route_output", route_output_flag);
			parser_settings.GetValueByFieldName("vehicle_output", vehicle_log_flag);
			parser_settings.GetValueByFieldName("simulation_output", simulation_output_flag);
			

			

			break;
		}

		parser_settings.CloseCSVFile();
	}

	return;
}

void createModeTypeFile(const std::string& fileName) {
	std::ofstream file(fileName);
	if (!file.is_open()) {
		std::cerr << "Could not create the file: " << fileName << std::endl;
		return;
	}

	// Writing the headers of the CSV
	file << "mode_type_id,mode_type,name,vot,pce,occ,demand_file\n";

	// Writing the sample data (from your provided file)
	file << "1,sov,DRIVE, 10, 1, 1,demand.csv\n";
	file << "2,hov,HOV, 10, 1, 2,demand_hov.csv\n";
	file << "3,trk,truck, 10, 2, 1,demand_trk.csv\n";

	std::cout << "sample_mode_type.csv file created successfully!" << std::endl;
}

void createDemandFile(const std::string& fileName) {
	std::ofstream file(fileName);
	if (!file.is_open()) {
		std::cerr << "Could not create the file: " << fileName << std::endl;
		return;
	}

	// Writing the headers of the CSV
	file << "o_zone_id,d_zone_id,volume\n";
	// Store path information for each destination
	for (int i = 1; i <= no_nodes; i++)
		for (int j = 1; j <= no_nodes; j++)
	{
			if (g_node_vector[i].zone_id > 0)
				if (g_node_vector[j].zone_id > 0)
		{
					file << g_node_vector[i].zone_id << "," << g_node_vector[j].zone_id << ",1," << endl;
				}
	}

	std::cout << "sample_demand.csv file created successfully!" << std::endl;
}

void read_mode_type_file()
{
	createModeTypeFile("sample_mode_type.csv");

	CDTACSVParser parser_mode_type;

	if (parser_mode_type.OpenCSVFile("mode_type.csv", true))
	{

		number_of_modes = 0;
		while (parser_mode_type.ReadRecord())  // if this line contains [] mark, then we will also read
			// field headers.
		{
			number_of_modes += 1;
			if (number_of_modes < MAX_MODE_TYPES)
			{
				g_mode_type_vector[number_of_modes].mode_type = "auto";
				g_mode_type_vector[number_of_modes].vot = 10;
				g_mode_type_vector[number_of_modes].pce = 1;
				g_mode_type_vector[number_of_modes].occ = 1; 
				g_mode_type_vector[number_of_modes].demand_file = "demand.csv";

				parser_mode_type.GetValueByFieldName("mode_type", g_mode_type_vector[number_of_modes].mode_type);
				parser_mode_type.GetValueByFieldName("vot", g_mode_type_vector[number_of_modes].vot);
				parser_mode_type.GetValueByFieldName("pce", g_mode_type_vector[number_of_modes].pce);
				parser_mode_type.GetValueByFieldName("occ", g_mode_type_vector[number_of_modes].occ);
				parser_mode_type.GetValueByFieldName("demand_file", g_mode_type_vector[number_of_modes].demand_file);

				g_mode_type_vector[number_of_modes].dedicated_shortest_path = 1;
				parser_mode_type.GetValueByFieldName("dedicated_shortest_path", g_mode_type_vector[number_of_modes].dedicated_shortest_path);

				if (number_of_modes == 1)
					g_mode_type_vector[1].dedicated_shortest_path = 1;  // reset; 

			}

		}

		parser_mode_type.CloseCSVFile();
	}

	if (number_of_modes == 0)  //default 
	{
		g_mode_type_vector[1].demand_file = "demand.csv";
		g_mode_type_vector[1].mode_type = "auto";
		g_mode_type_vector[1].vot = 10;
		g_mode_type_vector[1].pce = 1;
		g_mode_type_vector[1].occ = 1;
		g_mode_type_vector[1].dedicated_shortest_path = 1;  // reset; 
		number_of_modes = 1;
	}

	printf("number_of_modes = %d\n", number_of_modes);
	return;
}
// Initialize the 5D vector
void InitializeLinkIndices(int num_modes, int no_sequential_zones, int max_routes)
{
	

	auto start = std::chrono::high_resolution_clock::now();  // Start timing



	linkIndices = std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>>(
		num_modes + 1,
		std::vector<std::vector<std::vector<std::vector<int>>>>(
			no_sequential_zones + 1,
			std::vector<std::vector<std::vector<int>>>(
				no_sequential_zones + 1,
				std::vector<std::vector<int>>(
					max_routes + 1
				)
			)
		)
	);

	// Record the end time
	auto end = std::chrono::high_resolution_clock::now();

	// Calculate the duration in milliseconds

	// Calculate the duration in seconds
	auto duration = end - start;

	// Convert to hours, minutes, seconds
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration % std::chrono::seconds(1));

	printf("Memory creation time for 5D link path matrix: %lld hours %lld minutes %lld seconds %lld ms\n", hours.count(), minutes.count(), seconds.count(), milliseconds.count());
}

int AssignmentAPI()
{

	fopen_s(&summary_log_file, "summary_log_file.txt", "w");

	double* MainVolume, * SubVolume, * SDVolume, Lambda;
	int*** MDMinPathPredLink;

	read_settings_file();
	read_mode_type_file(); 


	fopen_s(&logfile, "TAP_log.csv", "w");  // Open the log file for writing.
	no_nodes = get_number_of_nodes_from_node_file(no_zones, FirstThruNode);


	if (first_through_node_id_input >= 1)
	{ 
			if (g_map_external_node_id_2_node_seq_no.find(first_through_node_id_input) !=
			g_map_external_node_id_2_node_seq_no.end())
		{
			FirstThruNode  = g_map_external_node_id_2_node_seq_no[first_through_node_id_input]; //equal to external input from seetings.csv
		}
		else
		{
			printf("cannot find node id matching first_through_node_id_input %d", first_through_node_id_input);
		}
	}else if (first_through_node_id_input == 0)
		FirstThruNode = 0; // 


	number_of_links = get_number_of_links_from_link_file();

	printf("# of nodes= %d, largest zone id (# of zones) = %d, First Through Node ID = %d, number of links = %d\n", no_nodes, no_zones,
		g_node_vector[FirstThruNode].node_id, number_of_links);

	fprintf(summary_log_file, "no_nodes= %d, no_zones = %d, FirstThruNode Node ID = %d, number_of_links = %d\n", no_nodes, no_zones,
		g_node_vector[FirstThruNode].node_id, number_of_links);

	fopen_s(&link_performance_file, "link_performance.csv", "w");
	if (link_performance_file == NULL)
	{
		printf("Error opening file!\n");
		return 1;
	}
	fclose(link_performance_file);

	fopen_s(&link_performance_file, "link_performance.csv", "a+");


	double system_wide_travel_time = 0;
	double system_least_travel_time = 0;

	//fprintf(logfile,
	//    "iteration_no,link_id,internal_from_node_id,internal_to_node_id,volume,capacity,voc,"
	//    "fftt,travel_time,delay\n");



	Init(number_of_modes, no_zones);

	if(g_accessibility_only_mode ==0 && route_output_flag) 
	{ 
		int no_sequential_zones =  g_map_external_node_id_2_node_seq_no[no_zones];
	
		 InitializeLinkIndices(number_of_modes, no_sequential_zones, TotalAssignIterations);
	}

	for (int p = 0; p <g_number_of_processors; p++)
		Processor_origin_zones[p].clear();

	for (int i = 1; i <= no_nodes; i++)
		{
			int p = i % g_number_of_processors;
			if(g_node_vector[i].zone_id>=0)
			{ 

			Processor_origin_zones[p].push_back(g_node_vector[i].node_id);
			}
		}



	if (g_accessibility_only_mode == 1)
	{
		number_of_internal_zones = 0;
		// Store path information for each destination
		for (int j = 1; j <= no_nodes; j++)
		{
			if (g_node_vector[j].internal_zone_no >= 0)
				number_of_internal_zones++;
		}

		if (number_of_internal_zones < 10000)
			ComputeAccessibilityAndODCosts_v1("od_performance.csv");
		else
		{
			rewriteFile("od_performance.csv", "");
			ComputeAccessibilityAndODCosts_v2("od_performance.csv");
		}

			return 0; 
		}


	int iteration_no = 0;
	MainVolume = (double*)Alloc_1D(number_of_links, sizeof(double));
	SDVolume = (double*)Alloc_1D(
		number_of_links, sizeof(double)); /* Compute search direction and sub-volume in the same place. */
	SubVolume = (double*)Alloc_1D(
		number_of_links, sizeof(double)); /* Compute search direction and sub-volume in the same place. */
	MDMinPathPredLink = (int***)Alloc_3D(number_of_modes, no_zones, no_nodes, sizeof(int));

	// Record the start time
	auto start = std::chrono::high_resolution_clock::now();

	for (int k = 1; k <= number_of_links; k++)
	{
		MainVolume[k] = Link[k].Base_demand_volume;  // assign the base volume  to main volume 
		SDVolume[k] = 0;
		SubVolume[k] = 0;
	}

	system_wide_travel_time = UpdateLinkCost(MainVolume);  // set up the cost first using FFTT

	fprintf(link_performance_file,
		"iteration_no,link_id,vdf_type,from_node_id,to_node_id,length_meter,vdf_length_mi,vehicle_volume,person_volume,ref_vhc_olume,base_demand_volume,obs_volume,background_volume,"
		"link_capacity,lane_capacity,D,doc,vdf_fftt,travel_time,vdf_distance_factor,vdf_alpha,vdf_beta,vdf_plf,speed_mph,speed_kmph,VMT,VHT,PMT,PHT,VHT_QVDF,PHT_QVDF,geometry,");

	fprintf(logfile, "iteration_no,link_id,from_node_id,to_node_id,volume,ref_volume,obs_volume,capacity,doc,fftt,travel_time,delay,");

	for (int m = 1; m <= number_of_modes; m++)
		fprintf(logfile, "mod_vol_%s,", g_mode_type_vector[m].mode_type.c_str());

	fprintf(logfile, "sub_main_vol,");

	for (int m = 1; m <= number_of_modes; m++)
		fprintf(logfile, "sub_mod_vol_%s,", g_mode_type_vector[m].mode_type.c_str());

	fprintf(logfile, "\n");


	for (int m = 1; m <= number_of_modes; m++)
		fprintf(link_performance_file, "mod_vol_%s,", g_mode_type_vector[m].mode_type.c_str());

	for (int m = 1; m <= number_of_modes; m++)
		fprintf(link_performance_file, "obs_vol_%s,", g_mode_type_vector[m].mode_type.c_str());

	fprintf(link_performance_file, "P,t0,t2,t3,vt2_mph,vt2_kmph,mu,Q_gamma,free_speed_mph,cutoff_speed_mph,free_speed_kmph,cutoff_speed_kmph,congestion_ref_speed_mph,avg_queue_speed_mph,avg_QVDF_period_speed_mph,congestion_ref_speed_kmph,avg_queue_speed_kmph,avg_QVDF_period_speed_kmph,avg_QVDF_period_travel_time,Severe_Congestion_P,");

	for (int t = demand_period_starting_hours * 60; t < demand_period_ending_hours * 60; t += 5)
	{
		int hour = t / 60;
		int minute = t - hour * 60;

		fprintf(link_performance_file, "spd_mph_%02d:%02d,", hour, minute);
	}

	fprintf(link_performance_file, "\n");

	system_least_travel_time = FindMinCostRoutes(MDMinPathPredLink);
	double gap;
	if(system_wide_travel_time>0)
	{
		gap = (system_wide_travel_time - system_least_travel_time) /
		(fmax(0.1, system_least_travel_time)) * 100;

	if (gap < 0)
	{
		int ii = 0;
	}

	printf("iter No = %d, sys. TT =  %lf, least TT =  %lf, gap = %f %%\n", iteration_no,
		system_wide_travel_time, system_least_travel_time, gap);
	fprintf(summary_log_file, "iter No = %d, sys. TT =  %lf, least TT =  %lf, gap = %f %%\n", iteration_no,
		system_wide_travel_time, system_least_travel_time, gap);
	}



	All_or_Nothing_Assign(iteration_no, MDDiffODflow, MDMinPathPredLink, MainVolume);  // here we use MDDiffODflow as our OD search direction of D^c - D^b,



	system_wide_travel_time = UpdateLinkCost(MainVolume);

	if (g_tap_log_file == 1)
	{
		for (int k = 1; k <= number_of_links; k++)
		{
			fprintf(logfile, "%d,%d,%d,%d,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,",
				iteration_no, k, Link[k].external_from_node_id, Link[k].external_to_node_id,
				MainVolume[k], Link[k].Ref_volume, Link[k].Obs_volume[1], Link[k].background_volume, Link[k].Link_Capacity,
				MainVolume[k] / fmax(0.01, Link[k].Link_Capacity), Link[k].FreeTravelTime,
				Link[k].Travel_time, Link[k].Travel_time - Link[k].FreeTravelTime);

			for (int m = 1; m <= number_of_modes; m++)
				fprintf(logfile, "%2lf,", Link[k].mode_MainVolume[m]);

			for (int m = 1; m <= number_of_modes; m++)
				fprintf(logfile, "%2lf,", Link[k].Obs_volume[m]);

			fprintf(logfile, "%2lf,", MainVolume[k]);

			for (int m = 1; m <= number_of_modes; m++)
				fprintf(logfile, "%2lf,", Link[k].mode_SubVolume[m]);

			fprintf(logfile, "\n");

		}
	}
	std::chrono::duration<double, std::milli> duration_FindMinCostRoutes;
	std::chrono::duration<double, std::milli> duration_All_or_Nothing_Assign;
	std::chrono::duration<double, std::milli> duration_LinksSDLineSearch;
	auto start0 = std::chrono::high_resolution_clock::now();  // Start timing


	std::vector<double> m_lambda;

	Lambda = 1;
	for (iteration_no = 1; iteration_no < TotalAssignIterations; iteration_no++)
	{
		system_least_travel_time = FindMinCostRoutes(MDMinPathPredLink);  // the one right before the assignment iteration 
		
		gap = (system_wide_travel_time - system_least_travel_time) /
			(fmax(0.1, system_least_travel_time)) * 100;


	

		if (gap < 0)
		{
			int ii = 0;
		}

		printf("iter No = %d, Lambda = %f, g_System_VMT = %.1f, sys. TT =  %.1f, least TT =  %.1f, gap = %f %%\n",
			iteration_no, Lambda, g_System_VMT, system_wide_travel_time, system_least_travel_time, gap);
		fprintf(summary_log_file, "iter No = %d, Lambda = %f, g_System_VMT = %f, sys. TT =  %lf, least TT =  %lf, gap = %f %% \n",
			iteration_no, Lambda, g_System_VMT, system_wide_travel_time, system_least_travel_time, gap);


		g_System_VMT = 0;

		for (int k = 1; k <= number_of_links; k++)
		{
			if (Link[k].link_type >= 1)  // only include physical links
			{
				g_System_VMT += MainVolume[k] * Link[k].length;
			}
		}

		
		All_or_Nothing_Assign(iteration_no, MDODflow, MDMinPathPredLink, SubVolume); // this uees D^c, subvolume is Y, 
		auto end1 = std::chrono::high_resolution_clock::now();    // End timing
		auto start2 = std::chrono::high_resolution_clock::now();  // Start timing

		VolumeDifference(SubVolume, MainVolume, SDVolume); /* Which yields the search direction. SDVolume = y-X */


		Lambda = LinksSDLineSearch(MainVolume, SDVolume);
	
		m_lambda.push_back(Lambda);
		// MSA options
	 //   Lambda = 1.0 / (iteration_no + 1);



		UpdateVolume(MainVolume, SDVolume, Lambda);  // x(k+1) = x(k) +lambda * (y-x) MainVolume is MainVolume
		system_wide_travel_time = UpdateLinkCost(MainVolume);

		//system_least_travel_time = FindMinCostRoutes(MinPathPredLink);  // the one right after the updated link cost 


		if (g_tap_log_file == 1)
		{
			for (int k = 1; k <= number_of_links; k++)
			{
				fprintf(logfile, "%d,%d,%d,%d,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,",
					iteration_no, k, Link[k].external_from_node_id, Link[k].external_to_node_id,
					MainVolume[k], Link[k].Ref_volume, Link[k].Lane_Capacity, Link[k].Link_Capacity,
					MainVolume[k] / fmax(0.01, Link[k].Link_Capacity), Link[k].FreeTravelTime,
					Link[k].Travel_time, Link[k].Travel_time - Link[k].FreeTravelTime);

				for (int m = 1; m <= number_of_modes; m++)
					fprintf(logfile, "%2lf,", Link[k].mode_MainVolume[m]);

				fprintf(logfile, "%2lf,", SDVolume[k]);

				for (int m = 1; m <= number_of_modes; m++)
					fprintf(logfile, "%2lf,", Link[k].mode_SDVolume[m]);

				fprintf(logfile, "\n");

			}
		}
		auto end2 = std::chrono::high_resolution_clock::now();    // End timing

	}


	// Record the end time
	auto end = std::chrono::high_resolution_clock::now();

	// Calculate the duration in milliseconds

	// Calculate the duration in seconds
	auto duration = end - start;

	// Convert to hours, minutes, seconds
	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration % std::chrono::minutes(1));
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration % std::chrono::seconds(1));

	printf("CPU running time: %lld hours %lld minutes %lld seconds %lld ms\n",
		hours.count(), minutes.count(), seconds.count(), milliseconds.count());

	// Log the result with hours, minutes, seconds, and milliseconds
	fprintf(summary_log_file, "CPU running time: %lld hours %lld minutes %lld seconds %lld ms\n",
		hours.count(), minutes.count(), seconds.count(), milliseconds.count());

	//for (int k = 1; k <= number_of_links; k++)
	//{
	//    fprintf(logfile, "%d,%d,%d,%d,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf\n", iteration_no, k,
	//        Link[k].external_from_node_id, Link[k].external_to_node_id, MainVolume[k],
	//        Link[k].Capacity, MainVolume[k] / fmax(0.01, Link[k].Capacity),
	//        Link[k].FreeTravelTime, Link[k].Travel_time,
	//        Link[k].Travel_time - Link[k].FreeTravelTime);
	//}



	// Compute the flow proportions (theta).
	std::vector<double> m_theta;
	if (m_lambda.size() == 0)
		m_lambda.push_back(1.0);

	m_theta = computeTheta(m_lambda);

	if (g_ODME_mode == 1)
		performODME(m_theta, MainVolume, Link);

	OutputODPerformance("od_performance.csv");

	GenerateAggregatedPerformanceAndAccessibility();

	if (route_output_flag)
	{
		OutputRouteDetails("route_assignment.csv", m_theta);

	}

	// output link_performance.csv

	for (int k = 1; k <= number_of_links; k++)
	{




		double P = 0;
		double vt2 = Link[k].Cutoff_Speed;
		double mu = Link[k].Lane_Capacity;
		double Severe_Congestion_P;
		double model_speed[300];
		double t0 = 0;
		double t2 = 0;
		double t3 = 0;
		double Q_gamma = 0;
		double congestion_ref_speed = 0;
		double avg_queue_speed = 0;
		double avg_QVDF_period_speed = 0;
		double IncomingDemand = 0;
		double DOC = 0;
		Link_QueueVDF(k, MainVolume[k], IncomingDemand, DOC, P, t0, t2, t3, vt2, mu, Q_gamma, congestion_ref_speed, avg_queue_speed, avg_QVDF_period_speed, Severe_Congestion_P, model_speed);


		double VMT, VHT, PMT, PHT, VHT_QVDF, PHT_QVDF;
		double person_volume = 0;

		VMT = 0; VHT = 0;  PMT = 0; PHT = 0; VHT_QVDF = 0; PHT_QVDF = 0;

		VMT += MainVolume[k] * Link[k].length;
		VHT += MainVolume[k] * Link[k].Travel_time / 60.0;
		VHT_QVDF += MainVolume[k] * Link[k].QVDF_TT / 60.0;

		for (int m = 1; m <= number_of_modes; m++)
		{

			person_volume += Link[k].mode_MainVolume[m]*g_mode_type_vector[m].occ;
		}

		PMT = person_volume * Link[k].length;
		PHT = person_volume * Link[k].Travel_time / 60.0;


		PHT_QVDF += person_volume * Link[k].QVDF_TT / 60.0;

		fprintf(link_performance_file, "%d,%d,%d,%d,%d,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,",
			iteration_no, Link[k].link_id, Link[k].vdf_type, Link[k].external_from_node_id, Link[k].external_to_node_id, Link[k].length*1609.0, Link[k].length,
			MainVolume[k], person_volume, Link[k].Ref_volume, Link[k].Base_demand_volume, Link[k].Obs_volume[1], Link[k].background_volume, Link[k].Link_Capacity, Link[k].Lane_Capacity, IncomingDemand, DOC, Link[k].FreeTravelTime,
			Link[k].Travel_time, Link[k].VDF_distance_factor, Link[k].VDF_Alpha, Link[k].VDF_Beta, Link[k].VDF_plf, Link[k].length / fmax(Link[k].Travel_time / 60.0, 0.001), Link[k].length / fmax(Link[k].Travel_time / 60.0, 0.001) * 1.609, Link[k].Travel_time - Link[k].FreeTravelTime);

		fprintf(link_performance_file, "%2lf,%2lf,%2lf,%2lf,%2lf,%2lf,", VMT, VHT, PMT, PHT, VHT_QVDF, PHT_QVDF);

		fprintf(link_performance_file, "\"%s\",",
			Link[k].geometry.c_str());

		for (int m = 1; m <= number_of_modes; m++)
			fprintf(link_performance_file, "%2lf,", Link[k].mode_MainVolume[m]);
	
		for (int m = 1; m <= number_of_modes; m++)
			fprintf(link_performance_file, "%2lf,", Link[k].Obs_volume[m]);

		fprintf(link_performance_file, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,", P, t0, t2, t3, vt2, vt2 * 1.609, mu, Q_gamma,
			Link[k].free_speed, Link[k].Cutoff_Speed, Link[k].free_speed * 1.609, Link[k].Cutoff_Speed * 1.609,
			congestion_ref_speed, avg_queue_speed, avg_QVDF_period_speed,
			congestion_ref_speed * 1.609, avg_queue_speed * 1.609, avg_QVDF_period_speed * 1.609,
			Link[k].QVDF_TT, Severe_Congestion_P);
		for (int t = demand_period_starting_hours * 60; t < demand_period_ending_hours * 60; t += 5)
		{
			int t_interval = t / 5;
			double speed = model_speed[t_interval];
			fprintf(link_performance_file, "%.3f,", speed);
		}



		fprintf(link_performance_file, "\n");
	}
	
	if(simulation_output_flag ==1)
			SimulationAPI();


	linkIndices.clear();
	linkIndices.shrink_to_fit();

	g_map_external_node_id_2_node_seq_no.clear();
	g_map_node_seq_no_2_external_node_id.clear();
	g_map_internal_zone_no_2_node_seq_no.clear();

	free(MainVolume);
	free(SubVolume);
	free(SDVolume);

	Free_3D((void***)MDMinPathPredLink, number_of_modes, no_zones, no_nodes);

	Close();

	fclose(link_performance_file);
	fclose(logfile);  // Close the log file when you're done with it.
	fclose(summary_log_file);

	return 0;
}

static void Init(int int_number_of_modes, int input_no_zones)
{
	// tuiInit(tuiFileName);
	InitLinks();
	createDemandFile("sample_demand.csv");
	if (g_accessibility_only_mode == 0)
	{
		baseODDemand_loaded_flag = Read_ODflow(&TotalODflow, &int_number_of_modes, &input_no_zones);
	}
	if (baseODDemand_loaded_flag == 0)
	{
		// reset 
		for (int k = 1; k <= number_of_links; k++)
		{

			Link[k].Base_demand_volume = 0;

			for (int m = 1; m <= int_number_of_modes; m++)
			{
				Link[k].mode_Base_demand_volume[m] = 0;
			}
		}
	}
}

static void Close()
{
	StatusMessage("General", "Closing all modules");
	// tuiClose(tuiFileName);
	CloseLinks();
	CloseODflow();

}


static void CloseODflow(void)
{

	free(TotalOFlow);

	Free_3D((void***)seed_MDODflow, number_of_modes, no_zones, no_zones);

	Free_3D((void***)MDODflow, number_of_modes, no_zones, no_zones);
	Free_3D((void***)targetMDODflow, number_of_modes, no_zones, no_zones);

	if (g_ODME_mode == 1)
	{
		Free_3D((void***)old_MDODflow, number_of_modes, no_zones, no_zones);
		Free_3D((void***)candidate_MDODflow, number_of_modes, no_zones, no_zones);
		Free_3D((void***)gradient_MDODflow, number_of_modes, no_zones, no_zones);
	}

	Free_3D((void***)MDDiffODflow, number_of_modes, no_zones, no_zones);
	Free_3D((void***)MDRouteCost, number_of_modes, no_zones, no_zones);

}






void ReadLinks()
{

	CDTACSVParser parser_link;
	std::vector<CLink> links;

	std::string line;

	if (parser_link.OpenCSVFile("link.csv", true))
	{
		float total_base_link_volume = 0;
		int line_no = 0;

		int k = 1;  // link start from 1

		while (parser_link.ReadRecord())  // if this line contains [] mark, then we will also read
			// field headers.
		{
			// CLink link;
			int lanes = 1;
			float capacity = 0;
			float free_speed = 10;

			parser_link.GetValueByFieldName("lanes", lanes);
			parser_link.GetValueByFieldName("capacity", capacity);
			parser_link.GetValueByFieldName("free_speed", free_speed);

			if (g_metric_system_flag == 1)
				free_speed = free_speed / 1.609;

			parser_link.GetValueByFieldName("vdf_free_speed_mph", free_speed);

			if (lanes <= 0 || capacity < 0.0001 || free_speed < 0.0001) {
				std::cerr << "Error: Invalid link attributes detected. "
					<< "lanes = " << lanes
					<< ", capacity = " << capacity
					<< ", free_speed = " << free_speed
					<< ". Skipping this link." << std::endl;
				continue;
			}


			Link[k].setup(number_of_modes);
			std::string value;



			// Read link_id
			parser_link.GetValueByFieldName("from_node_id", Link[k].external_from_node_id);
			parser_link.GetValueByFieldName("to_node_id", Link[k].external_to_node_id);
			parser_link.GetValueByFieldName("link_id", Link[k].link_id);

			Link[k].link_id = k; // rewrite using internal link id
			parser_link.GetValueByFieldName("link_type", Link[k].link_type);



			Link[k].internal_from_node_id = Link[k].external_from_node_id;
			Link[k].internal_to_node_id = Link[k].external_to_node_id;

			if (g_map_external_node_id_2_node_seq_no.find(Link[k].external_from_node_id) !=
				g_map_external_node_id_2_node_seq_no.end())
			{
				Link[k].internal_from_node_id =
				g_map_external_node_id_2_node_seq_no[Link[k].external_from_node_id];
			}
			else
			{
				printf("Error in from_node_id =%d for link_id = %d\n", Link[k].external_from_node_id, Link[k].link_id);

				continue; 
			}

			if (Link[k].internal_from_node_id == 0)
			{
				printf("Error in Link[k].internal_from_node_id\n"); 
			}
			if (g_map_external_node_id_2_node_seq_no.find(Link[k].external_to_node_id) !=
				g_map_external_node_id_2_node_seq_no.end())
			{
				Link[k].internal_to_node_id =
				g_map_external_node_id_2_node_seq_no[Link[k].external_to_node_id];
			}
			else
			{
				printf("Error in to_node_id =%d for link_id = %d\n", Link[k].external_to_node_id, Link[k].link_id);
				continue;
			}

			g_node_vector[Link[k].internal_to_node_id].m_incoming_link_seq_no_vector.push_back(k);
			g_node_vector[Link[k].internal_from_node_id].m_outgoing_link_seq_no_vector.push_back(k);

			parser_link.GetValueByFieldName("length", Link[k].length);  // meter
			Link[k].length = Link[k].length / 1609.0;  // miles

			double vdf_length_mi = -1; 
			parser_link.GetValueByFieldName("vdf_length_mi", vdf_length_mi);

			if (vdf_length_mi >= 0)  // valid. 
				Link[k].length = vdf_length_mi;

			parser_link.GetValueByFieldName("ref_volume", Link[k].Ref_volume);

			parser_link.GetValueByFieldName("non_uturn_flag", Link[k].non_uturn_flag);
			

			for (int m = 1; m <= number_of_modes; m++)
			{
				char CSV_field_name[50];

				if (number_of_modes == 1)
				{
					sprintf(CSV_field_name, "obs_volume");
				}
				else
				{
					sprintf(CSV_field_name, "obs_volume_%s", g_mode_type_vector[m].mode_type.c_str());
				}
				parser_link.GetValueByFieldName(CSV_field_name, Link[k].Obs_volume[m], false, false);

			}

			if (g_base_demand_mode == 1)
			{

				parser_link.GetValueByFieldName("base_demand_volume", Link[k].Base_demand_volume);
				total_base_link_volume += Link[k].Base_demand_volume;

				if (number_of_modes == 1)  // single mode 
					Link[k].mode_Base_demand_volume[1] = Link[k].Base_demand_volume;

				for (int m = 1; m <= number_of_modes; m++)
				{
					std::string field_name = "base_vol_" + std::string(g_mode_type_vector[m].mode_type);
					parser_link.GetValueByFieldName(field_name.c_str(), Link[k].mode_Base_demand_volume[m]);

				}
			}



			Link[k].lanes = lanes;
			Link[k].Lane_Capacity = capacity;
			Link[k].Link_Capacity = lanes * capacity;


			parser_link.GetValueByFieldName("allowed_use", Link[k].allowed_uses);

			if (g_tap_log_file == 1)
			{
				fprintf(logfile, "link %d->%d, node_seq_no %d->%d\n", Link[k].external_from_node_id, Link[k].external_to_node_id, Link[k].internal_from_node_id, Link[k].internal_to_node_id);

			}


			for (int m = 1; m <= number_of_modes; m++)
			{
				Link[k].mode_allowed_use[m] = 1;
				Link[k].mode_MainVolume[m] = 0;
				Link[k].mode_SubVolume[m] = 0;
				Link[k].mode_Toll[m] = 0;
				Link[k].mode_AdditionalCost[m] = 0;

			}

			if (Link[k].allowed_uses.size() > 0 && Link[k].allowed_uses != "all")
			{
				for (int m = 1; m <= number_of_modes; m++)
				{
					if (Link[k].allowed_uses.find(g_mode_type_vector[m].mode_type) != std::string::npos)  // otherwise, only an agent type is listed in this "allowed_uses", then this agent type is allowed to travel on this link
					{
						Link[k].mode_allowed_use[m] = 1;  // found 
					}
					else
					{
						Link[k].mode_allowed_use[m] = 0;
					}
				}


			}



			// Read internal_from_node_id

			// Read length

			// Read capacity

			Link[k].FreeTravelTime = Link[k].length / fmax(0.001, free_speed) * 60.0;

			parser_link.GetValueByFieldName("vdf_fftt", Link[k].FreeTravelTime,true);

			parser_link.GetValueByFieldName("vdf_type", Link[k].vdf_type, false);
			parser_link.GetValueByFieldName("vdf_distance_factor", Link[k].VDF_distance_factor, false);
			
			parser_link.GetValueByFieldName("vdf_alpha", Link[k].VDF_Alpha, true);
			parser_link.GetValueByFieldName("vdf_beta", Link[k].VDF_Beta, true);
			parser_link.GetValueByFieldName("vdf_plf", Link[k].VDF_plf, true);


			
			for (int m = 1; m <= number_of_modes; m++)
			{
				char CSV_field_name[50];

				if(number_of_modes == 1)
				{
					sprintf(CSV_field_name, "toll", g_mode_type_vector[m].mode_type.c_str());
				}
				else
				{ 
				sprintf(CSV_field_name, "toll_%s", g_mode_type_vector[m].mode_type.c_str());
				}
				parser_link.GetValueByFieldName(CSV_field_name, Link[k].mode_Toll[m], false, false);

				Link[k].mode_AdditionalCost[m] = Link[k].mode_Toll[m] / g_mode_type_vector[m].vot * 60.0;
			}



			if (capacity > 0)
				Link[k].BoverC = Link[k].VDF_Alpha / pow(capacity * lanes, Link[k].VDF_Beta);
			else
				Link[k].BoverC = 0;


			parser_link.GetValueByFieldName("vdf_fp", Link[k].Q_fp);
			parser_link.GetValueByFieldName("vdf_fd", Link[k].Q_fd);
			parser_link.GetValueByFieldName("vdf_n", Link[k].Q_n);
			parser_link.GetValueByFieldName("vdf_s", Link[k].Q_s);

			Link[k].free_speed = free_speed;
			Link[k].Cutoff_Speed = free_speed * 0.75;  // use 0.75 as default ratio, when free_speed = 70 mph, Cutoff_Speed = 52.8 mph in I-10 data set
			parser_link.GetValueByFieldName("geometry", Link[k].geometry, false);
			k++;
		}

		printf("total_base_link_volume = %f\n", total_base_link_volume);

		if (total_base_link_volume > 0)
			baselinkvolume_loaded_flag = 1;
		parser_link.CloseCSVFile();
	}



}


void Load_Movement_Restrictions(const std::string& filename)
{
	CDTACSVParser parser_settings;

	if (parser_settings.OpenCSVFile(filename, true)) // Open CSV file
	{
		while (parser_settings.ReadRecord()) // Read each record (row)
		{
			int mvmt_id = 0, node_id = 0, ib_link_id = 0, ob_link_id = 0;
			float penalty = 0; 

			parser_settings.GetValueByFieldName("mvmt_id", mvmt_id);
			parser_settings.GetValueByFieldName("node_id", node_id);
			parser_settings.GetValueByFieldName("ib_link_id", ib_link_id);
			parser_settings.GetValueByFieldName("ob_link_id", ob_link_id);
			parser_settings.GetValueByFieldName("penalty", penalty);

			// Check if the movement is restricted (e.g., U-turn or other restrictions)
			if (penalty>=10 && ib_link_id>=1 && ib_link_id <= number_of_links && ob_link_id >= 1 && ob_link_id <= number_of_links)
			{
				InsertMovementRestriction(ib_link_id, ob_link_id,true); 
				Link[ib_link_id].b_withmovement_restrictions = true; 
			}
		}

		parser_settings.CloseCSVFile(); // Close file after reading
	}
	else
	{
		cout << "Warning: Could not open " << filename << std::endl;
	}
}
static void InitLinkPointers(char* LinksFileName)
{
	int k, Node, internal_from_node_id;
	// Node is the internal node id
	FirstLinkFrom = (int*)Alloc_1D(no_nodes, sizeof(int));
	LastLinkFrom = (int*)Alloc_1D(no_nodes, sizeof(int));

	FirstLinkFrom[1] = 1;
	Node = 1;

	for (k = 1; k <= number_of_links; k++)
	{
		internal_from_node_id = Link[k].internal_from_node_id;
		if (internal_from_node_id == Node)
			continue;

		else if (internal_from_node_id >= Node + 1)
		{
			LastLinkFrom[Node] = k - 1;
			Node = internal_from_node_id;
			FirstLinkFrom[Node] = k;
		}

		else if (internal_from_node_id < Node)
		{
			// ExitMessage("Sort error in link file '%s': a link from node %d was found after "
			//	"a link from node %d \n", LinksFileName, internal_from_node_id, Node);
		}
		else if (internal_from_node_id > Node + 1)
		{
			// InputWarning("link file '%s' has no links out from "
			//	"nodes %d through %d. \n", LinksFileName, Node + 1, internal_from_node_id - 1);
			LastLinkFrom[Node] = k - 1;
			for (Node++; Node < internal_from_node_id; Node++)
			{
				FirstLinkFrom[Node] = 0;
				LastLinkFrom[Node] = -1;
			}
			FirstLinkFrom[Node] = k; /* Node equals internal_from_node_id now. */
		}
	}

	if (Node == no_nodes)
	{
		LastLinkFrom[Node] = number_of_links; /* Now Node equals no_nodes in any case */
	}
	else
	{
		// InputWarning("link file '%s' has no links out from "
		//	"nodes %d through %d. \n", LinksFileName, Node + 1, no_nodes);
		LastLinkFrom[Node] = k - 1;
		for (Node++; Node <= no_nodes; Node++)
		{
			FirstLinkFrom[Node] = 0;
			LastLinkFrom[Node] = -1;
		}
	}

	if (g_tap_log_file == 1)
	{
		for (Node = 1; Node <= no_nodes; Node++)
		{ 
			fprintf(logfile, "node_id = %d, FirstLinkFrom = %d, LastLinkFrom = %d \n", g_map_node_seq_no_2_external_node_id[Node], FirstLinkFrom[Node], FirstLinkFrom[Node]);

		}
	}

}


void InitLinks()
{
	char LinksFileName[100] = "link.csv";
	char* InterFlowFileName;
	FILE* LinksFile;
	int k;

	Link = (struct link_record*)Alloc_1D(number_of_links, sizeof(struct link_record));
	ReadLinks();

	InitLinkPointers(LinksFileName);
	UpdateLinkAdditionalCost();
	Load_Movement_Restrictions("movement.csv");
}



void StatusMessage(const char* group, const char* format, ...)
{
	double new_time;
}

int Read_ODtable(double*** ODtable, double*** DiffODtable, double*** Seed_ODtable, double*** target_ODtable, int no_zones)
{
	char ch, Coloumn[2], Semicoloumn[2]; /* Reserve room for the '\0' in the fscanf_s. */
	int Orig, Dest, NewOrig, NewDest;
	double Value;

	// current OD demand 

	int number_of_demand_files_read = 0; 
	for (int m = 1; m <= number_of_modes; m++)
	{
		FILE* file;
		fopen_s(&file, g_mode_type_vector[m].demand_file.c_str(), "r");
		printf("read demand file %s\n", g_mode_type_vector[m].demand_file.c_str());
		fprintf(summary_log_file, "read demand file %s\n", g_mode_type_vector[m].demand_file.c_str());

		if (file == NULL)
		{
			if (g_mode_type_vector[m].demand_file.length() > 0)
			{

				printf("Failed to open demand file %s\n", g_mode_type_vector[m].demand_file.c_str());

				if (number_of_modes == 1)
				{
					for (int o = 1; o <= no_zones; o++)
						for (int d = 1; d <= no_zones; d++)
						{

							if (g_map_external_node_id_2_node_seq_no.find(o) != g_map_external_node_id_2_node_seq_no.end()
								&& g_map_external_node_id_2_node_seq_no.find(d) != g_map_external_node_id_2_node_seq_no.end())
							{

								ODtable[m][o][d] = 1.0;
								Seed_ODtable[m][o][d] = 1.0;
								DiffODtable[m][o][d] = 1.0; // by default in case there is no baseline demand being specified 
							}
						}
				}
			}





				return 0;
		}

		int o_zone_id, d_zone_id;
		double volume;

		// Skip the header line
		char header[100];
		if (fgets(header, sizeof(header), file) == NULL)
		{
			printf("Failed to read header\n");
			return 0;
		}

		int line_count = 0;
		double total_volume = 0;
		// Read the data
		int result;
		while ((result = fscanf(file, "%d,%d,%lf", &o_zone_id, &d_zone_id, &volume)) != EOF)
		{
			if (o_zone_id > no_zones)
			{
				printf("o_zone_id: %d, d_zone_id: %d, volume: %.4lf\n", o_zone_id, d_zone_id,
					volume);
				printf("Error o_zone_id %d  > # of zones %d \n", o_zone_id, no_zones);
				break;
			}
			if (d_zone_id > no_zones)
			{
				printf("o_zone_id: %d, d_zone_id: %d, volume: %.4lf\n", o_zone_id, d_zone_id,
					volume);
				printf("Error d_zone_id %d  > # of zones %d \n", d_zone_id, no_zones);
				break;
			}


			ODtable[m][o_zone_id][d_zone_id] = volume;
			Seed_ODtable[m][o_zone_id][d_zone_id] = volume;
			DiffODtable[m][o_zone_id][d_zone_id] = volume; // by default in case there is no baseline demand being specified 
			total_volume += volume;

			if (result == 3)  // we have read all the 3 values correctly
			{
				if (line_count <= 3)
				{
					printf("o_zone_id: %d, d_zone_id: %d, volume: %.4lf\n", o_zone_id, d_zone_id,
						volume);

				}

				line_count++;
			}
			else
			{
				printf("Error reading line %d\n", line_count);
				break;
			}
		}

		printf(" mode type = %s, total_volume = %f\n", g_mode_type_vector[m].mode_type.c_str(), total_volume);
		fprintf(summary_log_file, " mode type = %s, total_volume = %f\n", g_mode_type_vector[m].mode_type.c_str(), total_volume);
		number_of_demand_files_read++;
		fclose(file);

	}

	// baseline OD demand --. diff OD  = current OD - baseline ODdemand 

	if (g_base_demand_mode == 1 && baselinkvolume_loaded_flag == 1)
	{


		for (int m = 1; m <= number_of_modes; m++)
		{
			FILE* file;
			std::string original_filename = g_mode_type_vector[m].demand_file;
			std::string modified_filename;

			// Check if the original filename ends with ".csv" and replace it with "_base.csv"
			size_t pos = original_filename.find(".csv");
			if (pos != std::string::npos) {
				modified_filename = original_filename.substr(0, pos) + "_base.csv";
			}
			else {
				// If the file does not have ".csv", append "_base"
				modified_filename = original_filename + "_base";
			}

			fopen_s(&file, modified_filename.c_str(), "r");
			printf("read base demand file %s\n", modified_filename.c_str());

			if (file == NULL)
			{
				// by default, we can skip this requirement, but if we load baseline link volume we should have base OD demand for consistency 
				break;
			}

			int o_zone_id, d_zone_id;
			double volume;

			// Skip the header line
			char header[100];
			if (fgets(header, sizeof(header), file) == NULL)
			{
				printf("Failed to read header\n");
				return 0;
			}

			int line_count = 0;
			double total_volume_diff = 0;
			// Read the data
			int result;
			while ((result = fscanf(file, "%d,%d,%lf", &o_zone_id, &d_zone_id, &volume)) != EOF)
			{
				if (result == 3)  // we have read all the 3 values correctly
				{
					if (line_count <= 3)
					{
						printf("o_zone_id: %d, d_zone_id: %d, volume: %.4lf\n", o_zone_id, d_zone_id,
							volume);

					}
					DiffODtable[m][o_zone_id][d_zone_id] = ODtable[m][o_zone_id][d_zone_id] - volume;  // diff OD demand  = current demand - base demand 
					total_volume_diff += ODtable[m][o_zone_id][d_zone_id] - volume;
					line_count++;
				}
				else
				{
					printf("Error reading line %d\n", line_count);
					break;
				}
			}

			printf(" mode type = %s, total_volume_diff = %f\n", g_mode_type_vector[m].mode_type.c_str(), total_volume_diff);

			fclose(file);

		}
	}

	// ODME mode

	if (g_ODME_mode == 1)
	{


		for (int m = 1; m <= number_of_modes; m++)
		{
			FILE* file;
			std::string original_filename = g_mode_type_vector[m].demand_file;
			std::string modified_filename;

			// Check if the original filename ends with ".csv" and replace it with "_base.csv"
			size_t pos = original_filename.find(".csv");
			if (pos != std::string::npos) {
				modified_filename = original_filename.substr(0, pos) + "_target.csv";
			}
			else {
				// If the file does not have ".csv", append "_target"
				modified_filename = original_filename + "_target";
			}

			fopen_s(&file, modified_filename.c_str(), "r");
			printf("read target demand file %s\n", modified_filename.c_str());

			if (file == NULL)
			{

				g_ODME_target_od = -1; 
				std::cerr << "Error:  ODME requires the target demand files " << modified_filename << std::endl;
				std::exit(1);
				// by default, we can skip this requirement, but if we load baseline link volume we should have base OD demand for consistency 

			}

			g_ODME_target_od = 1;

			int o_zone_id, d_zone_id;
			double volume;

			// Skip the header line
			char header[100];
			if (fgets(header, sizeof(header), file) == NULL)
			{
				printf("Failed to read header\n");
				return 0;
			}

			int line_count = 0;
			double total_volume_diff = 0;
			// Read the data
			int result;
			while ((result = fscanf(file, "%d,%d,%lf", &o_zone_id, &d_zone_id, &volume)) != EOF)
			{
				target_ODtable[m][o_zone_id][d_zone_id] = volume;  // target

				if (result == 3)  // we have read all the 3 values correctly
				{
					if (line_count <= 3)
					{
						printf("o_zone_id: %d, d_zone_id: %d, volume: %.4lf\n", o_zone_id, d_zone_id,
							volume);

					}


					line_count++;
				}
				else
				{
					printf("Error reading line %d\n", line_count);
					break;
				}
			}

			printf(" mode type = %s, total_volume_diff = %f\n", g_mode_type_vector[m].mode_type.c_str(), total_volume_diff);

			fclose(file);

		}
	}
	return 1;
}


double Link_Travel_Time_BPR(int k, double* Volume)
{

	double IncomingDemand = Volume[k] / fmax(0.01, Link[k].lanes) / fmax(0.001, demand_period_ending_hours - demand_period_starting_hours) / fmax(0.0001, Link[k].VDF_plf);

	Link[k].Travel_time =
		Link[k].FreeTravelTime * (1.0 + Link[k].VDF_Alpha * (pow(IncomingDemand / fmax(0.1, Link[k].Link_Capacity), Link[k].VDF_Beta)));

	if (Link[k].Travel_time < 0)
		Link[k].Travel_time = 0;
	Link[k].BPR_TT = Link[k].Travel_time;

	return (Link[k].Travel_time);
}
// helper: computes t/t0 = f(x; alpha, beta)
inline double travel_time_ratio(double x, double alpha, double beta) {
	double one_minus_x = 1.0 - x;
	return 2.0
		+ std::sqrt(alpha * alpha * one_minus_x * one_minus_x + beta * beta)
		- alpha * one_minus_x
		- beta;
}

double Link_Conical_Travel_Time(int k, double* Volume)
{
	double IncomingDemand = Volume[k] / fmax(0.01, Link[k].lanes) / fmax(0.001, demand_period_ending_hours - demand_period_starting_hours) / fmax(0.0001, Link[k].VDF_plf);

	double cap = fmax(0.1, Link[k].Link_Capacity);
	double x = IncomingDemand / cap;

	// compute the new travel-time ratio
	double ratio = travel_time_ratio(x, Link[k].VDF_Alpha, Link[k].VDF_Beta);

	// apply to free‐flow time
	Link[k].Travel_time = Link[k].FreeTravelTime * ratio;

	// guard against negative
	if (Link[k].Travel_time < 0.0)
		Link[k].Travel_time = 0.0;

	// store for legacy
	Link[k].BPR_TT = Link[k].Travel_time;

	return Link[k].Travel_time;
}


double Link_Travel_Time(int k, double* Volume)
{

	if (Link[k].vdf_type == 0)
		return Link_Travel_Time_BPR(k, Volume) + Link[k].VDF_distance_factor;
	if (Link[k].vdf_type == 1)
		return Link_Conical_Travel_Time(k, Volume) + Link[k].VDF_distance_factor;
}


double Link_QueueVDF(int k, double Volume, double& IncomingDemand, double& DOC, double& P, double& t0, double& t2, double& t3, double& vt2, double& Q_mu, double& Q_gamma, double& congestion_ref_speed,
	double& avg_queue_speed, double& avg_QVDF_period_speed, double& Severe_Congestion_P, double model_speed[300])
{

	IncomingDemand = Volume / fmax(0.01, Link[k].lanes) / fmax(0.001, demand_period_ending_hours - demand_period_starting_hours) / fmax(0.0001, Link[k].VDF_plf);
	DOC = IncomingDemand / fmax(0.1, Link[k].Lane_Capacity);

	double Travel_time =
		Link[k].FreeTravelTime * (1.0 + Link[k].VDF_Alpha * (pow(DOC, Link[k].VDF_Beta)));

	congestion_ref_speed = Link[k].Cutoff_Speed;
	if (DOC < 1)
		congestion_ref_speed = (1 - DOC) * Link[k].free_speed + DOC * Link[k].Cutoff_Speed;


	//step 3.2 calculate speed from VDF based on D/C ratio
	avg_queue_speed = congestion_ref_speed / (1.0 + Link[k].VDF_Alpha * pow(DOC, Link[k].VDF_Beta));


	P = Link[k].Q_fd * pow(DOC, Link[k].Q_n);  // applifed for both uncongested and congested conditions

	double H = demand_period_ending_hours - demand_period_starting_hours;

	if (P > H)
		avg_QVDF_period_speed = avg_queue_speed;
	else
		avg_QVDF_period_speed = P / H * avg_queue_speed + (1.0 - P / H) * (congestion_ref_speed + Link[k].free_speed) / 2.0;


	Link[k].QVDF_TT = Link[k].length / fmax(0.1, avg_QVDF_period_speed) * 60.0;






	double base = Link[k].Q_fp * pow(P, Link[k].Q_s) + 1.0;
	vt2 = Link[k].Cutoff_Speed / fmax(0.001, base);

	t2 = (demand_period_starting_hours + demand_period_ending_hours) / 2.0;
	t0 = t2 - 0.5 * P;
	t3 = t2 + 0.5 * P;

	Q_mu = std::min(Link[k].Lane_Capacity, IncomingDemand / std::max(0.01, P));

	//use  as the lower speed compared to 8/15 values for the congested states
	double RTT = Link[k].length / fmax(0.01, congestion_ref_speed);
	double wt2 = Link[k].length / vt2 - RTT; // in hour


	//step 5 compute gamma parameter is controlled by the maximum queue
	Q_gamma = wt2 * 64 * Q_mu / pow(P, 4);  // because q_tw = w*mu =1/4 * gamma (P/2)^4, => 1/vt2 * mu = 1/4 * gamma  * (P/2)^4


	double td_w = 0;
	//step scan the entire analysis period
	Severe_Congestion_P = 0;


	for (int t_in_min = demand_period_starting_hours * 60; t_in_min <= demand_period_ending_hours * 60; t_in_min += 5)  // 5 min interval
	{
		int t_interval = t_in_min / 5;
		double t = t_in_min / 60.0;  // t in hour
		double td_queue = 0;
		double td_speed = 0;
		model_speed[t_interval] = Link[k].free_speed;

		if (t0 <= t && t <= t3)  // within congestion duration P
		{
			//1/4*gamma*(t-t0)^2(t-t3)^2
			td_queue = 0.25 * Q_gamma * pow((t - t0), 2) * pow((t - t3), 2);
			td_w = td_queue / fmax(0.001, Q_mu);
			//L/[(w(t)+RTT_in_hour]
			td_speed = Link[k].length / (td_w + RTT);
		}
		else if (t < t0) //outside
		{
			td_queue = 0;
			double factor = (t - demand_period_starting_hours) / fmax(0.001, t0 - demand_period_starting_hours);
			td_speed = (1 - factor) * Link[k].free_speed + factor * fmax(congestion_ref_speed, avg_queue_speed);
		}
		else  // t> t3
		{
			td_queue = 0;
			double factor = (t - t3) / fmax(0.001, demand_period_ending_hours - t3);
			td_speed = (1 - factor) * fmax(congestion_ref_speed, avg_queue_speed) + (factor)*Link[k].free_speed;
		}

		// dtalog.output() << "td_queue t" << t << " =  " << td_queue << ", speed =" << td_speed << '\n';
		// g_DTA_log_file << "td_queue t" << t << " =  " << td_queue << ", speed =" << td_speed << '\n';



		if (t_in_min <= 410)
		{
			int idebug = 1;
		}
		double td_flow = 0; // default: get_volume_from_speed(td_speed, vf, k_critical, s3_m);
		model_speed[t_interval] = td_speed;

		if (td_speed < Link[k].free_speed * 0.5)
			Severe_Congestion_P += 5.0 / 60;  // 5 min interval
	}

	return P;
}

double Link_Travel_Time_Integral_BPR(int k, double* Volume)
{
	double IncomingDemand = Volume[k] / fmax(0.01, Link[k].lanes) / fmax(0.001, demand_period_ending_hours - demand_period_starting_hours) / fmax(0.0001, Link[k].VDF_plf);
	double integral = 0;
	if (Link[k].VDF_Beta >= 0.0)
		integral += IncomingDemand + (Volume[k] * Link[k].FreeTravelTime *
			(1.0 + (Link[k].BoverC / (Link[k].VDF_Beta + 1)) * pow(IncomingDemand, Link[k].VDF_Beta + 1)));

	return integral;
}

double Link_Travel_Time_Integral_Conical(int k, double* Volume)
{
	double t0 = Link[k].FreeTravelTime;                          // t₀
	double c = std::fmax(0.1, Link[k].Link_Capacity);           // capacity

	double IncomingDemand = Volume[k] / fmax(0.01, Link[k].lanes) / fmax(0.001, demand_period_ending_hours - demand_period_starting_hours) / fmax(0.0001, Link[k].VDF_plf);

	double x = IncomingDemand / c;                                     // normalized flow
	double α = Link[k].VDF_Alpha;
	double β = (2 * α - 1) / (2 * α - 2);                       // Spiess’s relation

	// helper
	double u = 1.0 - x;
	double S = std::sqrt(α * α * u * u + β * β);

	// primitive at x
	double Ix = 2.0 * x
		- 0.5 * u * S
		- (β * β) / (2 * α) * std::log(α * u + S)
		- α * (x - 0.5 * x * x)
		- β * x;

	// primitive at 0 (u=1, x=0)
	double u0 = 1.0;
	double S0 = std::sqrt(α * α * u0 * u0 + β * β);
	double I0 = 0.0   // 2·0
		- 0.5 * u0 * S0
		- (β * β) / (2 * α) * std::log(α * u0 + S0)
		- 0.0           // α·(0−0²/2)
		- 0.0;          // β·0

	// definitive integral
	return t0 * c * (Ix - I0);
}

double Link_Travel_Time_Integral(int k, double* Volume)
{
	double integral = 0; 

	if (Link[k].vdf_type == 0)
		integral =  Link_Travel_Time_Integral_BPR(k, Volume);

	if (Link[k].vdf_type == 1)
		integral =  Link_Travel_Time_Integral_Conical(k, Volume);

	return integral;
}


double Link_Travel_Time_Der(int k, double* Volume)
{
	if (Link[k].VDF_Beta == 0.0)
		return 0.0;
	else
		return (Link[k].FreeTravelTime * Link[k].BoverC * Link[k].VDF_Beta *
			pow(Volume[k], (Link[k].VDF_Beta - 1)));
}

double AdditionalCost(int k, int m)
{
	double AddCost = 0;

	AddCost = Link[k].mode_Toll[m] / g_mode_type_vector[m].vot * 60.0;

	return AddCost;
}

double Link_GenCost(int k, double* Volume)
{
	return (Link[k].mode_AdditionalCost[1] + Link_Travel_Time(k, Volume));
}

double LinkCost_Integral(int k, double* Volume)
{
	return (Link[k].mode_AdditionalCost[1] * Volume[k] + Link_Travel_Time_Integral(k, Volume));
}

double Link_GenCostDer(int k, double* Volume)
{
	return (Link_Travel_Time_Der(k, Volume));
}

/* External functions */

void ClearVolume(double* VolumeArray)
{
	int k;
	for (k = 1; k <= number_of_links; k++)
		VolumeArray[k] = 0.0;
}

void VolumeDifference(double* Volume1, double* Volume2, double* Difference)
{  // SubVolume, MainVolume
	int k;
	for (k = 1; k <= number_of_links; k++)
	{
		Difference[k] = Volume1[k] - Volume2[k];

		for (int m = 1; m <= number_of_modes; m++)
		{
			Link[k].mode_SDVolume[m] = Link[k].mode_SubVolume[m] - Link[k].mode_MainVolume[m];

			if (fabs(Difference[k] - Link[k].mode_SDVolume[m]) > 0.01)
			{
				printf("");
			}
		}
	}
}

void UpdateVolume(double* MainVolume, double* SDVolume, double Lambda)
{
	int k;
	for (k = 1; k <= number_of_links; k++)
	{
		MainVolume[k] += Lambda * SDVolume[k];
	}


	for (int k = 1; k <= number_of_links; k++)
	{
		for (int m = 1; m <= number_of_modes; m++)
		{
			Link[k].mode_MainVolume[m] += Lambda * Link[k].mode_SDVolume[m];
		}
	}
}

void UpdateLinkAdditionalCost(void)
{
	int k;

	for (k = 1; k <= number_of_links; k++)
		for (int m = 1; m <= number_of_modes; m++)
			AdditionalCost(k, m);
}

double UpdateLinkCost(double* MainVolume)
{
	int k;
	double system_wide_travel_time = 0;

	for (k = 1; k <= number_of_links; k++)
	{
		Link[k].Travel_time = Link_Travel_Time(k, MainVolume);

		Link[k].GenCost = Link_GenCost(k, MainVolume);
		system_wide_travel_time += (MainVolume[k] * Link[k].Travel_time);
	}

	return system_wide_travel_time;
}

void UpdateLinkCostDer(double* MainVolume)
{
	int k;

	for (k = 1; k <= number_of_links; k++)
	{
		Link[k].GenCostDer = Link_GenCostDer(k, MainVolume);
	}
}

void GetLinkTravelTimes(double* Volume, double* TravelTime)
{
	int k;

	for (k = 1; k <= number_of_links; k++)
	{
		TravelTime[k] = Link_Travel_Time(k, Volume);
	}
}

double TotalLinkCost(double* Volume)
{
	int k;
	double Sum = 0;

	for (k = 1; k <= number_of_links; k++)
		Sum += Link[k].GenCost * Volume[k];
	return Sum;
}

double OFscale = 1.0;

double OF_Links(double* MainVolume)
{
	int k;
	double Sum = 0;

	for (k = 1; k <= number_of_links; k++)
		Sum += LinkCost_Integral(k, MainVolume);

	return Sum / OFscale;
}

double OF_LinksDirectionalDerivative(double* MainVolume, double* SDVolume, double Lambda)
{
	//
	//purpose:
	//    this function calculates the directional derivative of the objective function with respect to the step size lambda.in optimization, the directional derivative indicates how the objective function changes as you move in a specific direction.
	//
	//        parameters :
	//        mainvolume : current flow volumes on each link.
	//        sdvolume : search direction volumes(difference between aon assignment and current flows).
	//        lambda : step size parameter indicating how far to move along the search direction.

		//Return Normalized Directional Derivative :
		//Normalize the sum by OFscale to get the directional derivative :
		//DirectionalDerivative
		//    =
		//    LinkCostSum
		//    OFscale
		//    DirectionalDerivative =
		//    OFscale
		//    LinkCostSum
		//    ?

		//    Mathematical Interpretation :
		//The directional derivative represents how the total system cost changes as you adjust the flow along the search direction by Lambda.In the context of traffic assignment, it helps determine whether increasing or decreasing Lambda will reduce the overall congestion and travel time.

	int k;
	double* Volume;
	double LinkCostSum = 0;

	Volume = (double*)Alloc_1D(number_of_links, sizeof(double));

	for (k = 1; k <= number_of_links; k++)
	{
		Volume[k] = MainVolume[k] + Lambda * SDVolume[k];
	}
	for (k = 1; k <= number_of_links; k++)
	{
		LinkCostSum += Link_GenCost(k, Volume) * SDVolume[k];

		//   LinkCostSum += Link_Travel_Time_Integral(k, Volume) * SDVolume[k];

	}

	free(Volume);
	return LinkCostSum / OFscale;
}


double Sum_ODtable(double*** ODtable, double* total_o_table, int no_zones)
{
	int Orig, Dest;
	double sum = 0.0;

	for (Orig = 1; Orig <= no_zones; Orig++)
		total_o_table[Orig] = 0;

	for (int m = 1; m <= number_of_modes; m++)
		for (Orig = 1; Orig <= no_zones; Orig++)
			for (Dest = 1; Dest <= no_zones; Dest++)
			{
				total_o_table[Orig] += ODtable[m][Orig][Dest];
				sum += ODtable[m][Orig][Dest];
			}


	return (sum);
}

int Read_ODflow(double* TotalODflow, int* number_of_modes, int* no_zones)
{
	FILE* ODflowFile;
	double RealTotal, InputTotal;

	MDODflow = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));
	seed_MDODflow = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));

	if (g_ODME_mode == 1)
	{
		old_MDODflow = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));
		candidate_MDODflow = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));
		gradient_MDODflow = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));
	}

	targetMDODflow = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));
		 
	TotalOFlow = (double*)Alloc_1D(*no_zones, sizeof(double));

	MDDiffODflow = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));

	MDRouteCost = (double***)Alloc_3D(*number_of_modes, *no_zones, *no_zones, sizeof(double));

	int with_basedemand = Read_ODtable(MDODflow, MDDiffODflow, seed_MDODflow, targetMDODflow, *no_zones);

	RealTotal = (double)Sum_ODtable(MDODflow, TotalOFlow, *no_zones);


	zone_outbound_link_size = (int*)Alloc_1D(*no_zones, sizeof(int));

	for (int n = 1; n <= *no_zones; n++)
	{
		zone_outbound_link_size[n] = 0;
	}
	for (int k = 1; k <= number_of_links; k++)
	{
		int from_node_id = Link[k].external_from_node_id;
		if (from_node_id <= *no_zones)
		{
			zone_outbound_link_size[from_node_id] += 1;  // from_node_id is the zone_id; 
		}
	}

	

	// checking 
	int total_infeasible_outbound_zones = 0; 
	float total_infeasible_outbound_zone_demand = 0;
	float total_zone_demand = 0;
	for (int z = 1; z < *no_zones; z++)
	{
		if (zone_outbound_link_size[z] == 0 && TotalOFlow[z]>0.01)
		{
			printf("Error: There is no outbound link from zone %d with positive demand %f\n", z, TotalOFlow[z]);
			total_infeasible_outbound_zones++; 
			total_infeasible_outbound_zone_demand += TotalOFlow[z];
		}
		total_zone_demand += TotalOFlow[z];
	}

	if(total_infeasible_outbound_zones)
	{
	printf("Error: %d zones have no outbound link with positive demand %f, = %f percentage of total demand\n", total_infeasible_outbound_zones,
		total_infeasible_outbound_zone_demand, total_infeasible_outbound_zone_demand*100/fmax(0.01, total_zone_demand));
	}

	*TotalODflow = (double)RealTotal;

	return with_basedemand;

}

void CloseLinks(void)
{
	int Node;
	free(zone_outbound_link_size);
	free(Link);
	free(FirstLinkFrom);
	free(LastLinkFrom);

}

double LinksSDLineSearch(double* MainVolume, double* SDVolume) {
	int n;
	double lambdaleft = 0, lambdaright = 1, lambda = 0.5;
	double grad;

	// Initial check at lambda = 0
	grad = OF_LinksDirectionalDerivative(MainVolume, SDVolume, 0.0);
	StatusMessage("Line search step", "0.0");
	StatusMessage("Line search grad", "%lf", grad);
	if (grad >= 0) {
		LastLambda = 0.0;
		return 0.0;
	}

	// Check at lambda = 1
	grad = OF_LinksDirectionalDerivative(MainVolume, SDVolume, 1.0);
	StatusMessage("Line search step", "1.0");
	StatusMessage("Line search grad", "%lf", grad);
	if (grad <= 0) {
		LastLambda = 1.0;
		return 1.0;
	}

	// Bisection method for line search within [0, 1]
	for (n = 1; n <= MinLineSearchIterations; n++) {
		grad = OF_LinksDirectionalDerivative(MainVolume, SDVolume, lambda);

		if (grad <= 0.0) {
			lambdaleft = lambda;
		}
		else {
			lambdaright = lambda;
		}

		lambda = 0.5 * (lambdaleft + lambdaright);
	}

	// Additional iterations to ensure convergence, if necessary
	for (; lambdaleft == 0 && n <= MAX_NO_BISECTITERATION; n++) {
		grad = OF_LinksDirectionalDerivative(MainVolume, SDVolume, lambda);

		if (grad <= 0.0) {
			lambdaleft = lambda;
		}
		else {
			lambdaright = lambda;
		}

		lambda = 0.5 * (lambdaleft + lambdaright);
	}

	ActualIterations = n - 1;
	LastLambda = lambdaleft;
	return lambdaleft;
}

/// <summary>
///  below is simulation codes
/// </summary>
/// 
/// 
/// 
/// 
// Add at the beginning of your code
enum LogLevel {
	LOG_ERROR = 0,    // Critical errors
	LOG_WARN = 1,     // Warnings
	LOG_INFO = 2,     // Important simulation events
	LOG_DEBUG = 3,    // Detailed debugging info
	LOG_TRACE = 4     // Most detailed level
};

class SimulationLogger {
private:
	ofstream debug_file;
	ofstream info_file;
	ofstream error_file;
	LogLevel current_level;

	static const char* getLevelString(LogLevel level) {
		switch (level) {
		case LOG_ERROR: return "ERROR";
		case LOG_WARN:  return "WARN";
		case LOG_INFO:  return "INFO";
		case LOG_DEBUG: return "DEBUG";
		case LOG_TRACE: return "TRACE";
		default: return "UNKNOWN";
		}
	}

public:
	SimulationLogger(LogLevel level = LOG_INFO) : current_level(level) {
		debug_file.open("sim_debug.csv");
		info_file.open("sim_info.csv");
		error_file.open("sim_error.log");

		// Write headers
		debug_file << "timestamp,level,event,agent_id,link_id,node_id,details\n";
		info_file << "timestamp,level,event,count,details\n";
	}

	void log(LogLevel level, const string& event, int agent_id, int link_id, int node_id, const string& details) {
		if (level > current_level) return;

		string timestamp = to_string(time(nullptr));

		// Always log errors to error file
		if (level == LOG_ERROR) {
			error_file << timestamp << " [ERROR] " << event << " Agent:" << agent_id
				<< " Link:" << link_id << " Node:" << node_id << " - " << details << endl;
		}

		// Detailed debugging info
		if (level <= LOG_DEBUG) {
			debug_file << timestamp << "," << getLevelString(level) << "," << event << ","
				<< agent_id << "," << link_id << "," << node_id << "," << details << endl;
		}

		// Summary information
		if (level <= LOG_INFO) {
			info_file << timestamp << "," << getLevelString(level) << "," << event << ","
				<< "," << details << endl;
		}
	}

	void logAgentMovement(int t, int agent_id, int from_link, int to_link, const string& status) {
		if (LOG_TRACE <= current_level) {
			debug_file << t << ",TRACE,MOVE," << agent_id << "," << from_link << "->"
				<< to_link << "," << status << endl;
		}
	}

	void logQueueStatus(int t, int link_id, int entrance_size, int exit_size) {
		if (LOG_DEBUG <= current_level) {
			debug_file << t << ",DEBUG,QUEUE," << link_id << "," << entrance_size
				<< "," << exit_size << endl;
		}
	}

	void logSimulationStatus(int t, int loaded, int transfers, int completed) {
		if (LOG_INFO <= current_level) {
			info_file << t << ",INFO,STATUS," << loaded << "," << transfers
				<< "," << completed << endl;
		}
	}

	~SimulationLogger() {
		debug_file.close();
		info_file.close();
		error_file.close();
	}
};

//// Add to ParallelQueueSimulator class:
//SimulationLogger logger;
//
//// Then in your simulation methods, add logging:
//logger.logEvent(t, "agent_load", agent->agent_uid, l,
//	link.entrance_queue.size(), "Agent loaded to link");
class CAgent_Simu {
public:
	int agent_uid;
	int o_zone_id, d_zone_id;
	double distannce; 
	double travel_time; 
	int agent_seq_no; 
	double departure_time_in_min;
	int departure_time_in_simu_interval; 
	vector<int> path_link_sequence;
	vector<int> path_node_sequence;	
	vector<double> path_time_sequence;

	int current_link_seq_no; 
	bool loaded;
	double desired_free_travel_time_ratio;

	vector<double> link_arrival_times;  // in simu time interval
	vector<double> link_departure_times; // in simu time interval

	CAgent_Simu() :
		agent_uid(-1),
		agent_seq_no (-1),
		distannce (0),
		travel_time(0),
		departure_time_in_min(0),
		departure_time_in_simu_interval (0),
		current_link_seq_no(0),
		loaded(false),
		desired_free_travel_time_ratio(1.0) {}

	void initializeTimes(int path_size) {
		link_arrival_times.resize(path_size);
		link_departure_times.resize(path_size);
	}
};

int g_ParserIntSequence(std::string str, std::vector<int>& vect)
{
	std::stringstream ss(str);
	int i;
	while (ss >> i)
	{
		vect.push_back(i);
		if (ss.peek() == ';')
			ss.ignore();
	}

	return vect.size();
}
int read_vehicle_file(vector<shared_ptr<CAgent_Simu>>& agents) {
	CDTACSVParser parser_vehicle;
	int count = 0;

	if (!parser_vehicle.OpenCSVFile("vehicle.csv", true)) {
		cerr << "Error: Cannot open vehicle.csv" << endl;
		return 0;
	}

	while (parser_vehicle.ReadRecord()) {
		int agent_id;
		double departure_time;  // in min 
		string link_ids;
		string node_ids;

		if (!parser_vehicle.GetValueByFieldName("agent_id", agent_id) ||
			!parser_vehicle.GetValueByFieldName("departure_time", departure_time) ||
			!parser_vehicle.GetValueByFieldName("link_ids", link_ids)		||
			!parser_vehicle.GetValueByFieldName("node_ids", node_ids)
			) {
			continue;
		}

		auto agent = make_shared<CAgent_Simu>();
		agent->agent_uid = agent_id;
		agent->departure_time_in_min = departure_time;
	 
		vector<int> path_sequence;  // store link sequence id k 
		vector<int> path_node_sequence;  // store link sequence id k 

		if (g_ParserIntSequence(link_ids, path_sequence) > 0) {
			agent->path_link_sequence = path_sequence;

			if (g_ParserIntSequence(node_ids, path_node_sequence) > 0) {
				agent->path_node_sequence = path_node_sequence;
			}

			agent->path_time_sequence.push_back(departure_time);

			float arrival_time = departure_time;
			for (int l = 0; l < path_sequence.size(); l++)
			{
				int link_id = agent->path_link_sequence[l];
				arrival_time += Link[link_id].FreeTravelTime;
				agent->path_time_sequence.push_back(arrival_time);

			}

			agent->initializeTimes(path_sequence.size());
			agent->agent_seq_no = agents.size();
			parser_vehicle.GetValueByFieldName("o_zone_id", agent->o_zone_id);
			parser_vehicle.GetValueByFieldName("d_zone_id", agent->d_zone_id);
			parser_vehicle.GetValueByFieldName("total_distance_mile", agent->distannce);
			agents.push_back(agent);
			count++;
		}
	}

	// Sorting agents by departure time in ascending order
	sort(agents.begin(), agents.end(), [](const shared_ptr<CAgent_Simu>& a, const shared_ptr<CAgent_Simu>& b) {
		return a->departure_time_in_min < b->departure_time_in_min;
		});



	parser_vehicle.CloseCSVFile();

	//// Print the number of vehicles loaded
	//cout << "Number of vehicles loaded: " << count << endl;
	return count;
}

// Global state management
struct SimulationState {
	// Per-second state variables
	vector<vector<double>> linkOutFlowCapacity;    // State per second
	vector<vector<double>> linkOutFlowState;       // State per second

	// Per-minute state variables 
	vector<vector<double>> linkCumulativeArrival;  // Aggregated per minute
	vector<vector<double>> linkCumulativeDeparture;// Aggregated per minute
	vector<vector<double>> linkWaitingTime;        // Tracked per minute

	void initialize(int numLinks, int demandPeriodStartHours, int demandPeriodEndHours) {
		const int BUFFER_HOURS = 2;  // 120 minutes buffer
		int intervalsPerMin = (demandPeriodEndHours - demandPeriodStartHours + BUFFER_HOURS) * 60;
		int intervalsPerSec = intervalsPerMin * 60;

		// Initialize per-second arrays
		linkOutFlowCapacity.resize(numLinks, vector<double>(intervalsPerSec));
		linkOutFlowState.resize(numLinks, vector<double>(intervalsPerSec));

		// Initialize per-minute arrays
		linkCumulativeArrival.resize(numLinks, vector<double>(intervalsPerMin));
		linkCumulativeDeparture.resize(numLinks, vector<double>(intervalsPerMin));
		linkWaitingTime.resize(numLinks, vector<double>(intervalsPerMin));
	}

	void cleanup() {
		linkOutFlowCapacity.clear();
		linkOutFlowState.clear();
		linkCumulativeArrival.clear();
		linkCumulativeDeparture.clear();
		linkWaitingTime.clear();
	}
};

class TrafficSimulator {
private:
	SimulationState state;
	int numLinks;
	int numIntervals;
	const double SECONDS_PER_INTERVAL = 0.1;  // 6 seconds

	void initializeMemory() {
		state.initialize(numLinks, demand_period_starting_hours, demand_period_ending_hours);
	}

	void processSignalizedIntersections() {
		for (int link = 0; link < numLinks; ++link) {
			if (!Link[link].timing_arc_flag) continue;

			int cycleLength = Link[link].cycle_length;
			int startGreen = Link[link].start_green_time;
			int endGreen = Link[link].end_green_time;

			if (endGreen < startGreen) {
				endGreen += cycleLength;
			}

			processSignalTimings(link, cycleLength, startGreen, endGreen);
		}
	}

	void processSignalTimings(int link, int cycleLength, int startGreen, int endGreen) {
		int numCycles = (demand_period_ending_hours - demand_period_starting_hours) * 60 / fmax(1.0f, cycleLength);

		for (int cycle = 0; cycle < numCycles; ++cycle) {
			for (int t = cycle * cycleLength + startGreen;
				t <= cycle * cycleLength + endGreen; t++) {

				float saturationFlow = Link[link].Lane_SaturationFlowRate *
					Link[link].lanes / 3600.0;

				state.linkOutFlowCapacity[link][t] = (t % 2 == 0) ?
					Link[link].lanes : 0;
				state.linkOutFlowState[link][t] = 1;
			}
		}
	}

public:
	TrafficSimulator(int links, int intervals) : numLinks(links), numIntervals(intervals) {
		initializeMemory();
	}

	~TrafficSimulator() {
		state.cleanup();
	}

};
int number_of_seconds_per_interval = 6; 
class LinkQueue {
public:
	deque<int> loading_entrance_queue;       // Queue of vehicle IDs waiting to enter at loading stage
	deque<int> entrance_queue;       // Queue of vehicle IDs waiting to enter
	deque<int> exit_queue;          // Queue of vehicle IDs waiting to exit
	double capacity_per_time_step;   // Maximum number of vehicles that can enter/exit per time step
	double free_flow_time;          // Free flow travel time in minutes
	int number_of_lanes;            // Number of lanes
	double length;                  // Link length
	double spatial_capacity;        // Maximum number of vehicles that can be on the link

	LinkQueue() :
		capacity_per_time_step(0),
		free_flow_time(0),
		number_of_lanes(1),
		length(0),
		spatial_capacity(0) {}

	void reset() {
		entrance_queue.clear();
		exit_queue.clear();
	}

	void setup(link_record link_record_element)
	{
		free_flow_time = link_record_element.FreeTravelTime;
		number_of_lanes = link_record_element.lanes;
		length = link_record_element.length;   // internal unit: miles
		capacity_per_time_step = link_record_element.Link_Capacity / 3600;
		double kjam = 300; 
		spatial_capacity = link_record_element.length * kjam;
	}
};

class AgentLogger {
private:
	ofstream agent_file;

	string convertTimeToHHMMSS(double time_in_min) {
		int hours = (int)(time_in_min / 60);
		int minutes = (int)(time_in_min) % 60;
		int seconds = (int)((time_in_min - (int)time_in_min) * 60);

		char buffer[9];
		sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
		return string(buffer);
	}

	string vectorToString(const vector<int>& vec, const string& delimiter = ";") {
		stringstream ss;
		for (size_t i = 0; i < vec.size(); ++i) {
			if (i > 0) ss << delimiter;
			ss << vec[i];
		}
		return ss.str();
	}


	string doubleVectorToString(const vector<double>& vec, const string& delimiter = ";") {
		stringstream ss;
		for (size_t i = 0; i < vec.size(); ++i) {
			if (i > 0) ss << delimiter;
			ss << vec[i];
		}
		return ss.str();
	}

	string formatTimesVector(const vector<double>& times, int l_demand_period_starting_hours, const string& delimiter = ";") {
		stringstream ss;
		for (size_t i = 0; i < times.size(); ++i) {
			if (i > 0) ss << delimiter;
			if (times[i] >= 0) {
				ss << convertTimeToHHMMSS(l_demand_period_starting_hours*60.0+ times[i] * number_of_seconds_per_interval / 60.0);
			}
			else {
				ss << "NA";
			}
		}
		return ss.str();
	}

public:
	AgentLogger() {
		agent_file.open("trajectory.csv");
		// Write header
		agent_file << "agent_id,departure_time,departure_time_hhmmss,loaded_status,o_zone_id,d_zone_id,distance,travel_time,"
			<< "current_link_seq_no,link_ids,node_sequence,departure_time,arrival_time,time_sequence,geometry\n";
	}


	string getTrajectoryGeometry(const vector<int>& path_link_sequence) {
		stringstream ss;

		ss << fixed << setprecision(7);  // Fixed notation with 7 decimal places
		ss << "\"LINESTRING (";

		bool first_point = true;
		for (int link_id : path_link_sequence) {
			// Get from node of link
			if (!first_point) {
				ss << ", ";
			}

			int from_node = Link[link_id].internal_from_node_id;
			ss << g_node_vector[from_node].x << " " << g_node_vector[from_node].y;

			// Add to node only for the last link
			if (link_id == path_link_sequence.back()) {
				int to_node = Link[link_id].internal_to_node_id;
				ss << ", " << g_node_vector[to_node].x << " " << g_node_vector[to_node].y;
			}

			first_point = false;
		}
		ss << ")\"";
		return ss.str();
	}
	void logAgent(const shared_ptr<CAgent_Simu>& agent, int l_demand_period_starting_hours) {



		double travel_time = (agent->link_departure_times[agent->current_link_seq_no] -
			agent->link_arrival_times[0]) * number_of_seconds_per_interval / 60.0; 

		agent_file << agent->agent_uid << ","
			<< agent->departure_time_in_min << ","
			<< convertTimeToHHMMSS(agent->departure_time_in_min) << ","
			<< (agent->loaded ? "1" : "0") << ","
			<< agent->o_zone_id << "," 
			<< agent->d_zone_id << ","
			<< agent->distannce << ","
			<< travel_time << ","
			<< agent->current_link_seq_no << ","
			<< vectorToString(agent->path_link_sequence) << ","
			<< vectorToString(agent->path_node_sequence) << ","
			<< formatTimesVector(agent->link_arrival_times, l_demand_period_starting_hours) << ","
			<< formatTimesVector(agent->link_departure_times, l_demand_period_starting_hours) << ","
			<< doubleVectorToString(agent->path_time_sequence) << ","
			<< getTrajectoryGeometry(agent->path_link_sequence) << "\n";
	}

	void logAllAgents(const vector<shared_ptr<CAgent_Simu>>& agents, int l_demand_period_starting_hours) {
		for (const auto& agent : agents) {
			logAgent(agent,l_demand_period_starting_hours);
		}
		agent_file.flush();
	}

	~AgentLogger() {
		agent_file.close();
	}
};

class ParallelQueueSimulator {
public: 
	SimulationLogger logger;
	// Add debugging counters
	int total_loaded_agents = 0;
	int total_link_transfers = 0;
	int total_node_transfers = 0;
	int total_completed_trips = 0;
	ofstream debug_file;


	SimulationState state;
	vector<LinkQueue> link_queues;
	vector<shared_ptr<CAgent_Simu>> agents;
	int num_links;
	int num_nodes;

	void setupLink_queue()
	{
		for (int l = 1; l <= num_links; l++) {
					link_queues[l].setup(Link[l]);
					}
	}


	double calculateOutflowCapacity(int link_id, int time_step, int total_intervals) {
		const LinkQueue& link = link_queues[link_id];
		double base_capacity;

		// 1. Basic capacity calculation based on lanes and flow rate
		float discharge_rate_per_sec = link.capacity_per_time_step;
		base_capacity = discharge_rate_per_sec * number_of_seconds_per_interval;
		int BUFFER_HOURS = 2;  // 120 minutes buffer
		// 4. Special conditions handling
		if (total_intervals - time_step <= BUFFER_HOURS * 60 * number_of_seconds_per_interval) {
			// After loading period, increase capacity to clear network
			base_capacity *= 10;  // Multiplier for quick network clearance
		}
		//// 2. Signal timing handling
		//if (g_link_vector[link_id].timing_arc_flag) {
		//	int cycle_length = g_link_vector[link_id].cycle_length;
		//	int start_green = g_link_vector[link_id].start_green_time;
		//	int end_green = g_link_vector[link_id].end_green_time;

		//	if (end_green < start_green) {
		//		end_green += cycle_length;  // Handle wraparound
		//	}

		//	int time_in_cycle = time_step % cycle_length;
		//	if (time_in_cycle >= start_green && time_in_cycle <= end_green) {
		//		// Green phase - use saturation flow rate
		//		float saturation_flow_rate = g_link_vector[link_id].saturation_flow_rate *
		//			link.number_of_lanes / 3600.0;
		//		base_capacity = saturation_flow_rate * number_of_seconds_per_interval;
		//	}
		//	else {
		//		// Red phase
		//		return 0;
		//	}
		//}

		//// 3. Capacity reduction handling (incidents, work zones, etc.)
		//if (!g_link_vector[link_id].capacity_reduction_map.empty()) {
		//	int current_time_in_min = time_step * number_of_seconds_per_interval / 60;
		//	auto it = g_link_vector[link_id].capacity_reduction_map.find(current_time_in_min);

		//	if (it != g_link_vector[link_id].capacity_reduction_map.end()) {
		//		base_capacity *= it->second;  // Apply reduction factor
		//	}
		//}



		//// 5. Queue spillback check
		//if (link.exit_queue.size() >= link.spatial_capacity) {
		//	// If link is at spatial capacity, reduce outflow
		//	base_capacity = 0;
		//}

		// 6. Randomization for more realistic flow
		unsigned int RandomSeed = 101 + time_step;  // Simple seed based on time
		float residual = base_capacity - floor(base_capacity);
		RandomSeed = (LCG_a * RandomSeed + LCG_c) % LCG_M;  // Linear Congruential Generator
		float random_ratio = float(RandomSeed) / LCG_M;

		if (random_ratio < residual) {
			base_capacity = ceil(base_capacity);
		}
		else {
			base_capacity = floor(base_capacity);
		}

		// 7. Final bounds check
		return max(0.0, base_capacity);  // Maximum 2 vehicles per lane per interval
	}
	void estimateLinkCapacities(int t,  int total_intervals) {

		for (int l = 1; l <= num_links; l++) {
			double outflow_capacity = calculateOutflowCapacity(l, t , total_intervals);
			state.linkOutFlowCapacity[l][t] = outflow_capacity;
		}
	}

	void synchronizeBottlenecksFromNodetoLink(int t) {
#pragma omp parallel for
		for (int i = 1; i <= num_nodes; i++) {
			auto& node = g_node_vector[i];
			double total_demand = 0;

			// Calculate total demand
			for (int link_id : node.m_incoming_link_seq_no_vector) {
				total_demand += link_queues[link_id].exit_queue.size();
			}

			//if (total_demand >= 1)
			//{
			//	// Distribute capacity if bottleneck exists
			//	for (int link_id : node.m_incoming_link_seq_no_vector) {
			//		if (total_demand > state.linkOutFlowCapacity[link_id][t]) {
			//			double capacity_ratio = state.linkOutFlowCapacity[link_id][t] / total_demand;
			//			state.linkOutFlowCapacity[link_id][t] *= capacity_ratio;
			//		}
			//	}
			//}
		}
	}

	void logSimulationStatus(int t) {
		int t_in_min = t * number_of_seconds_per_interval / 60;
		if (t % (60/number_of_seconds_per_interval) == 0) {  // Log every minute

			debug_file << t << ",status," << total_loaded_agents << ","
				<< total_link_transfers << "," << total_node_transfers
				<< ",minute_summary\n";

			printf("\n=== Simulation Time: %d minutes ===\n", t_in_min);
			printf( "Total agents loaded: %d\n", total_loaded_agents);
			printf( "Total link transfers: %d\n", total_link_transfers);
			printf("Total node transfers: %d\n", total_node_transfers);
			printf( "Total completed trips: %d\n", total_completed_trips);



		}
	}

	void loadNewAgentsPerSimuInterval(int t) {
		int agents_loaded_this_step = 0;
		for (int l = 1; l <= num_links; l++) {
			LinkQueue& link = link_queues[l];
			while (!link.loading_entrance_queue.empty()) {
				int agent_seq_no = link.loading_entrance_queue.front();
				auto agent = agents[agent_seq_no];

				if (agent->departure_time_in_simu_interval == t) {
					link.loading_entrance_queue.pop_front();
					link.entrance_queue.push_back(agent->agent_seq_no);
					agent->link_arrival_times[agent->current_link_seq_no] = t;

					total_loaded_agents++;
					agents_loaded_this_step++;


				}
				else {
					break;
				}
			}
		}

		
	}

	void processNodeTransfers(int t) {
		int t_in_min = t * number_of_seconds_per_interval / 60;

#pragma omp parallel for
		for (int i = 1; i <= num_nodes; i++) {
			auto& node = g_node_vector[i];
			int node_transfers = 0;

			for (int link_id : node.m_incoming_link_seq_no_vector) {
				auto& link = link_queues[link_id];

				while (!link.exit_queue.empty()) {
					int agent_seq_no = link.exit_queue.front();
					auto agent = agents[agent_seq_no];

					if (agent->link_departure_times[agent->current_link_seq_no] > t) {
						break;
					}

					if (agent->current_link_seq_no >= agent->path_link_sequence.size() - 1) {
						// Agent completed trip
						link.exit_queue.pop_front();
#pragma omp atomic
						total_completed_trips++;
						debug_file << t << ",complete," << agent->agent_uid << ","
							<< link_id << ",0,trip_completed\n";
						continue;
					}

					int next_link_id = agent->path_link_sequence[agent->current_link_seq_no + 1];
					auto& next_link = link_queues[next_link_id];

					if (state.linkOutFlowCapacity[link_id][t] >= 1 &&
						next_link.entrance_queue.size() < next_link.capacity_per_time_step) {

						link.exit_queue.pop_front();
						next_link.entrance_queue.push_back(agent_seq_no);

						agent->current_link_seq_no++;
						agent->link_arrival_times[agent->current_link_seq_no] = t;

						node_transfers++;
#pragma omp atomic
						total_node_transfers++;

						debug_file << t << ",transfer," << agent->agent_uid << ","
							<< link_id << "->" << next_link_id << ","
							<< next_link.entrance_queue.size() << ",node_transfer\n";
					}
					else {
						// Log capacity constraint
						debug_file << t << ",blocked," << agent->agent_uid << ","
							<< link_id << "," << next_link.entrance_queue.size()
							<< ",capacity_constraint\n";
						break;
					}
				}
			}

			if (node_transfers > 0) {
#pragma omp critical
				{
					fprintf(summary_log_file, "Time %d: Node %d processed %d transfers\n",
						t, i, node_transfers);
				}
			}
		}
	}


	void processLinkTransfer(int t) {

		int t_in_min = t * number_of_seconds_per_interval / 60; 
#pragma omp parallel for
		for (int l = 1; l <= num_links; l++) {
			auto& link = link_queues[l];

			while (!link.entrance_queue.empty()) {
				int agent_seq_no = link.entrance_queue.front();
				auto agent = agents[agent_seq_no];

				double arrival_time_in_simu_interval = t;
				double free_flow_time = link.free_flow_time * agent->desired_free_travel_time_ratio;
				agent->link_departure_times[agent->current_link_seq_no] =
					t + (int)(free_flow_time * 60 / number_of_seconds_per_interval);


				
				link.entrance_queue.pop_front();
				link.exit_queue.push_back(agent_seq_no);
				//state.linkCumulativeArrival[l][t_in_min]++;
			}
		}
	}

public:
	ParallelQueueSimulator(int links_count, int nodes_count)
		: num_links(links_count), num_nodes(nodes_count) {
		link_queues.resize(num_links + 1);
		state.initialize(num_links + 1, demand_period_starting_hours, demand_period_ending_hours);

		// Initialize debug file
		debug_file.open("simulation_debug.csv");
		debug_file << "time,event_type,agent_id,link_id,queue_size,details\n";
	}

	~ParallelQueueSimulator() {
		debug_file.close();
	}

	void simulate(int total_intervals, int l_demand_period_starting_hours) {
		AgentLogger agent_logger;
		for (int t = 0; t < total_intervals; t++) 
		{
			loadNewAgentsPerSimuInterval(t);
			estimateLinkCapacities(t, total_intervals);
			synchronizeBottlenecksFromNodetoLink(t);
			processNodeTransfers(t);
			processLinkTransfer(t);
			logSimulationStatus(t);

			if (t>=600 && total_completed_trips > 0 && total_completed_trips == total_loaded_agents)
				break;
		}

		agent_logger.logAllAgents(agents, l_demand_period_starting_hours);
	}
	void loadAgents(const vector<shared_ptr<CAgent_Simu>>& agent_list, int demand_starting_time_in_simu_inteval) {
		if(summary_log_file!=NULL)
		{	
			fprintf(summary_log_file, "\n=== Loading Agents ===\n");
		fprintf(summary_log_file, "Total agents to load: %zu\n", agent_list.size());
		fprintf(summary_log_file, "Demand starting time in simulation interval: %d\n", demand_starting_time_in_simu_inteval);
		}

		agents = agent_list;
		int agents_with_empty_path = 0;
		map<int, int> link_loading_counts;  // Track how many agents are assigned to each first link

		for (auto& agent : agents) {
			agent->loaded = false;
			agent->current_link_seq_no = 0;

			// Log agent initial setup
			debug_file << "0,init," << agent->agent_uid << ",0,0,departure_time="
				<< agent->departure_time_in_min << "\n";

			if (!agent->path_link_sequence.empty()) {
				int first_link = agent->path_link_sequence[0];
				agent->departure_time_in_simu_interval =
					(int)(agent->departure_time_in_min * 60 / number_of_seconds_per_interval) - demand_starting_time_in_simu_inteval;

				link_queues[first_link].loading_entrance_queue.push_back(agent->agent_seq_no);
				link_loading_counts[first_link]++;

				// Log successful agent assignment
				debug_file << "0,assign," << agent->agent_uid << "," << first_link << ","
					<< link_queues[first_link].loading_entrance_queue.size()
					<< ",departure_interval=" << agent->departure_time_in_simu_interval << "\n";
			}
			else {
				agents_with_empty_path++;
				// Log error for agents with empty paths
				debug_file << "0,error," << agent->agent_uid << ",0,0,empty_path\n";
			}
		}


		//// Log link loading distribution
		//fprintf(summary_log_file, "\nInitial link loading distribution:\n");
		//for (const auto& [link_id, count] : link_loading_counts) {
		//	fprintf(summary_log_file, "Link %d: %d agents\n", link_id, count);
		//	debug_file << "0,link_summary," << link_id << ",0," << count
		//		<< ",initial_loading\n";
		}

}; 
int SimulationAPI()
{


	InitializeLinkIndices(number_of_modes, no_zones, TotalAssignIterations);
	int total_simu_time_intervals = (demand_period_ending_hours - demand_period_starting_hours) * 3600 / number_of_seconds_per_interval;

	vector<shared_ptr<CAgent_Simu>> agents;
	read_vehicle_file(agents);
	ParallelQueueSimulator simulator(number_of_links, no_nodes);
	simulator.setupLink_queue();
	simulator.loadAgents(agents, demand_period_starting_hours*3600/number_of_seconds_per_interval);
	simulator.simulate(total_simu_time_intervals, demand_period_starting_hours);

	return 0;
}


//// map_matching.cpp
//#include <cmath>
//#include <algorithm>
//#include <vector>
//#include <map>
//#include <string>
//#include <iostream>
//#include <cstdio>
//#include <cstdlib>

namespace MapMatching {

	// Constants
	constexpr double PI = 3.141592653589793;
	constexpr double EARTH_RADIUS = 6371000; // in meters
	constexpr double DEFAULT_GRID_RESOLUTION = 0.01; // adjust as needed

	// ---------------------------------------------------------
	// Geometry Utilities
	// ---------------------------------------------------------
	inline double toRadians(double degrees) {
		return degrees * PI / 180.0;
	}

	// Haversine distance (meters) given two lat/lon pairs.
	double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
		lat1 = toRadians(lat1); lon1 = toRadians(lon1);
		lat2 = toRadians(lat2); lon2 = toRadians(lon2);
		double dlat = lat2 - lat1;
		double dlon = lon2 - lon1;
		double a = std::pow(std::sin(dlat / 2), 2) +
			std::cos(lat1) * std::cos(lat2) * std::pow(std::sin(dlon / 2), 2);
		double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
		return EARTH_RADIUS * c;
	}

	// A simple point structure.
	struct GDPoint {
		double x, y;
	};

	// Euclidean (L2) distance between two points.
	inline double getEuclideanDistance(const GDPoint* p1, const GDPoint* p2) {
		return std::sqrt(std::pow(p1->x - p2->x, 2) + std::pow(p1->y - p2->y, 2));
	}

	// Compute distance from a point to a line segment [fromPt, toPt].
	double getPointToLineDistance(const GDPoint* pt,
		const GDPoint* fromPt,
		const GDPoint* toPt,
		double unitGridRes,
		bool no_intersection_requirement)
	{
		double lineLength = getEuclideanDistance(fromPt, toPt);
		if (lineLength < 1e-6)
			return getEuclideanDistance(pt, fromPt);
		double U = ((pt->x - toPt->x) * (fromPt->x - toPt->x) +
			(pt->y - toPt->y) * (fromPt->y - toPt->y))
			/ (lineLength * lineLength);
		if (!no_intersection_requirement && (U < 0.0 || U > 1.0))
			return unitGridRes; // fallback value
		GDPoint intersection;
		intersection.x = toPt->x + U * (fromPt->x - toPt->x);
		intersection.y = toPt->y + U * (fromPt->y - toPt->y);
		double d1 = getEuclideanDistance(pt, &intersection);
		double d0 = getEuclideanDistance(pt, fromPt);
		double d2 = getEuclideanDistance(pt, toPt);
		return no_intersection_requirement ? std::min({ d1, d0, d2 }) : d1;
	}

	// ---------------------------------------------------------
	// Data Structures for MMLinks and GPS Points.
	// ---------------------------------------------------------
	// A link (named MMLink) is defined by its two endpoints.
	struct MMLink {
		GDPoint fromPt;
		GDPoint toPt;
		int external_from_node_id;
		int external_to_node_id;


		double link_distance; // precomputed total length
		bool bInsideFlag;     // true if the link is within a grid cell with GPS points
		double likelihood_distance; // computed matching cost
		int likely_trace_no;        // matching GPS trace id
		int hit_count;              // count of “hits” from intersection tests
		MMLink(const GDPoint& f, const GDPoint& t, const int& from_node_id, const int& to_node_id)
			: fromPt(f), toPt(t), external_from_node_id(from_node_id), external_to_node_id(to_node_id),
			link_distance(getEuclideanDistance(&f, &t)),
			bInsideFlag(false),
			likelihood_distance(1e9),
			likely_trace_no(-1),
			hit_count(0) {}
	};

	// A GPS trace point.
	struct GPSPoint {
		GDPoint pt;
		int trace_no;
		int global_time;   // in seconds
		GPSPoint() : trace_no(0), global_time(0) {}
	};

	// ---------------------------------------------------------
	// Grid Cell for Matching.
	// ---------------------------------------------------------
	struct GridCell {
		double x, y;  // cell origin (e.g., lower-left)
		std::vector<int> linkIndices; // indices of MMLinks overlapping this cell
		std::vector<int> OriginZoneNodeIndices; // indices of MMLinks overlapping this cell
		std::vector<int> DestinationZoneNodeIndices; // indices of MMLinks overlapping this cell
		std::vector<GPSPoint> gpsPoints; // GPS points falling in this cell
		bool dwell_flag = false;
		float start_min = 1e9; // earliest GPS time (minutes) in cell
		float end_min = 0;     // latest GPS time (minutes)
		float dwell_time = 0;  // computed dwell time (minutes)
	};

	// ---------------------------------------------------------
	// MatchingGrid: builds a grid for MMLinks, marks visited cells from GPS traces,
	// and processes each cell to compute a matching cost.
	// ---------------------------------------------------------
	class MatchingGrid {


	public:

		int m_FirstThruNode;

		MatchingGrid(double left, double right, double bottom, double top, double gridRes = DEFAULT_GRID_RESOLUTION, int first_through_node_no = 0)
			: left_(left), right_(right), bottom_(bottom), top_(top), gridRes_(gridRes), m_FirstThruNode(first_through_node_no)
		{
			cols_ = static_cast<int>(std::ceil((right_ - left_) / gridRes_)) + 1;
			rows_ = static_cast<int>(std::ceil((top_ - bottom_) / gridRes_)) + 1;
			grid_.resize(rows_, std::vector<GridCell>(cols_));
			for (int r = 0; r < rows_; ++r)
				for (int c = 0; c < cols_; ++c) {
					grid_[r][c].x = left_ + c * gridRes_;
					grid_[r][c].y = bottom_ + r * gridRes_;
				}
		}

		// Add this method to retrieve a grid cell by (x, y)
		const GridCell* getCell(double x, double y) const {
			int c = static_cast<int>((x - left_) / gridRes_);
			int r = static_cast<int>((y - bottom_) / gridRes_);
			if (r >= 0 && r < rows_ && c >= 0 && c < cols_)
				return &grid_[r][c];
			return nullptr;
		}

		// Insert a MMLink into every cell overlapping its bounding box.
		void insertLink(int linkIndex, const GDPoint& fromPt, const GDPoint& toPt, int internal_from_node_id, int internal_to_node_id) {
			double minX = std::min(fromPt.x, toPt.x);
			double maxX = std::max(fromPt.x, toPt.x);
			double minY = std::min(fromPt.y, toPt.y);
			double maxY = std::max(fromPt.y, toPt.y);
			int colStart = getColIndex(minX);
			int colEnd = getColIndex(maxX);
			int rowStart = getRowIndex(minY);
			int rowEnd = getRowIndex(maxY);
			for (int r = rowStart; r <= rowEnd; ++r)
				for (int c = colStart; c <= colEnd; ++c)
					if (isValidCell(r, c))
					{
						grid_[r][c].linkIndices.push_back(linkIndex);

						if(internal_from_node_id< m_FirstThruNode)  // connector, internal_from_node_id is the TAZ centroid 
							grid_[r][c].OriginZoneNodeIndices.push_back(internal_from_node_id);
						if (internal_to_node_id < m_FirstThruNode)  // connector, internal_to_node_id is the TAZ centroid 
							grid_[r][c].DestinationZoneNodeIndices.push_back(internal_to_node_id);
					}
		}

		// Insert a GPS point and update the cell's time bounds.
		void insertGPSPoint(const GPSPoint& gps, const GDPoint& pt) {
			int r = getRowIndex(pt.y);
			int c = getColIndex(pt.x);
			if (isValidCell(r, c)) {
				grid_[r][c].gpsPoints.push_back(gps);
				float timeMin = gps.global_time / 60.0f;
				grid_[r][c].start_min = std::min(grid_[r][c].start_min, timeMin);
				grid_[r][c].end_min = std::max(grid_[r][c].end_min, timeMin);
			}
		}

		// Insert a GPS point and update the cell's time bounds.
		void cleanGPSPoint() {
			for (int r = 0; r < rows_; ++r) {
				for (int c = 0; c < cols_; ++c) {
					grid_[r][c].gpsPoints.clear();
				}
			}
		}
		// Process visited cells:
		// 1) Mark dwell cells (if time span > 120 minutes).
		// 2) For each link in a cell, loop over GPS points and update a matching cost.
		// 3) For links with no matching GPS point, assign the closest one.
		void processCells(const std::vector<MMLink>& links,
			std::vector<double>& linkGeneralCost,
			std::vector<int>& linkMatchingTraceNo,
			double nonHitDistanceRatio = 10.0)
		{
			// First pass: mark dwell cells.
			for (int r = 0; r < rows_; ++r) {
				for (int c = 0; c < cols_; ++c) {
					GridCell& cell = grid_[r][c];
					if (!cell.gpsPoints.empty() && (cell.end_min - cell.start_min > 120)) {
						cell.dwell_flag = true;
						cell.dwell_time = cell.end_min - cell.start_min;
					}
				}
			}
			// Second pass: update link cost from GPS points.
			for (int r = 0; r < rows_; ++r) {
				for (int c = 0; c < cols_; ++c) {
					GridCell& cell = grid_[r][c];
					if (!cell.gpsPoints.empty() && !cell.linkIndices.empty()) {
						// Initialize each link’s cost to the grid resolution.
						for (int linkIdx : cell.linkIndices)
							linkGeneralCost[linkIdx] = gridRes_;
						// For each GPS point…
						for (const auto& gps : cell.gpsPoints) {
							for (int linkIdx : cell.linkIndices) {
								const MMLink& L = links[linkIdx];
								double d = getPointToLineDistance(&gps.pt, &L.fromPt, &L.toPt, 1.0, true);
								double cost = d; // Here, you might combine with other measures.
								if (cost < linkGeneralCost[linkIdx]) {
									linkGeneralCost[linkIdx] = cost;
									linkMatchingTraceNo[linkIdx] = gps.trace_no;
								}
							}
						}
						// Stage 2: for links with no matching GPS point, find the closest one.
						for (int linkIdx : cell.linkIndices) {
							if (linkMatchingTraceNo[linkIdx] < 0) {
								double min_d = 1e9;
								int bestTrace = -1;
								for (const auto& gps : cell.gpsPoints) {
									double d = getPointToLineDistance(&gps.pt, &links[linkIdx].fromPt, &links[linkIdx].toPt, 1.0, false);
									if (d < min_d) { min_d = d; bestTrace = gps.trace_no; }
								}
								linkMatchingTraceNo[linkIdx] = bestTrace;
							}
						}
					}
				}
			}
		}

		void printSummary() const {
			std::cout << "MatchingGrid: " << rows_ << " rows x " << cols_ << " cols, resolution "
				<< gridRes_ << "\n";
			std::cout << "Bounds: x[" << left_ << ", " << right_ << "], y[" << bottom_ << ", " << top_ << "]\n";
		}

	private:
		int getColIndex(double x) const { return static_cast<int>((x - left_) / gridRes_); }
		int getRowIndex(double y) const { return static_cast<int>((y - bottom_) / gridRes_); }
		bool isValidCell(int r, int c) const { return r >= 0 && r < rows_ && c >= 0 && c < cols_; }
		double left_, right_, bottom_, top_, gridRes_;
		int rows_, cols_;
		std::vector<std::vector<GridCell>> grid_;
	};

	// Define a structure to hold trace record information.
	struct TraceRecord {
		int trace_no = 0;
		std::string trace_id = "";
		std::string allowed_link_type_code = "";
		std::string blocked_link_type_code = "";
		double y_coord = 0.0;
		double x_coord = 0.0;
		int road_order = 0;
		double reference_speed = 0.0;
		std::string agent_id = "";
		int internal_road_order = 0;
		int o_node_id = -1;
		int d_node_id = -1;
	};

	// Here we change the parameter to a nested map:
	// Outer key: agent_id (std::string)
	// Inner key: trace_no (int)
	// Value: vector of GPSPoints that belong to that trace.
	int readGPSTraceFile(std::unordered_map<std::string, std::vector<GPSPoint>>& gpsTraces)

	{
		CDTACSVParser parser_trace;
		std::vector<TraceRecord> traceRecords;

		// Open the CSV file (the second parameter 'true' indicates that the file contains headers)
		if (parser_trace.OpenCSVFile("trace.csv", true))
		{
			// Read each record until end-of-file.
			while (parser_trace.ReadRecord())
			{
				TraceRecord rec;
				// Read the fields by name.
				parser_trace.GetValueByFieldName("trace_no", rec.trace_no);
				parser_trace.GetValueByFieldName("trace_id", rec.trace_id);
				parser_trace.GetValueByFieldName("allowed_link_type_code", rec.allowed_link_type_code);
				parser_trace.GetValueByFieldName("blocked_link_type_code", rec.blocked_link_type_code);
				parser_trace.GetValueByFieldName("y_coord", rec.y_coord);
				parser_trace.GetValueByFieldName("x_coord", rec.x_coord);
				parser_trace.GetValueByFieldName("road_order", rec.road_order);
				parser_trace.GetValueByFieldName("reference_speed", rec.reference_speed);
				parser_trace.GetValueByFieldName("agent_id", rec.agent_id);
				parser_trace.GetValueByFieldName("internal_road_order", rec.internal_road_order);
				parser_trace.GetValueByFieldName("o_node_id", rec.o_node_id);
				parser_trace.GetValueByFieldName("d_node_id", rec.d_node_id);

				traceRecords.push_back(rec);
			}
			parser_trace.CloseCSVFile();
		}
		else
		{
			std::cerr << "Error: Cannot open trace.csv" << std::endl;
			std::exit(1);
		}

		// Group the GPS traces by agent_id and trace_no
		for (const auto& rec : traceRecords)
		{
			GPSPoint gps;
			gps.pt.x = rec.x_coord;
			gps.pt.y = rec.y_coord;
			gps.trace_no = rec.trace_no;
			// If you have a global_time field in your CSV, you can set gps.global_time here.

			// Group by agent_id then trace_no:
			gpsTraces[rec.agent_id].push_back(gps);
		}

		return 0;
	}


	// Helper: Compute Euclidean distance between two points.
	inline double computeEuclideanDistance(const GDPoint& a, const GDPoint& b) {
		return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
	}

	// Given a set of candidate zone node IDs (from a grid cell) and a point,
	// return the candidate whose coordinate is closest.
	// zoneNodeCoords: a mapping from zone node id to its coordinate.
	int determineZoneNode(const std::vector<int>& candidateNodeIds, const GDPoint& avgPt,
		const std::map<int, GDPoint>& zoneNodeCoords)
	{
		double minDist = std::numeric_limits<double>::max();
		int bestNode = -1;
		for (int nodeId : candidateNodeIds) {
			auto it = zoneNodeCoords.find(nodeId);
			if (it != zoneNodeCoords.end()) {
				double d = computeEuclideanDistance(avgPt, it->second);
				if (d < minDist) {
					minDist = d;
					bestNode = nodeId;
				}
			}
		}
		return bestNode;
	}

	// This function uses the first few and last few GPS points of a trace
	// to determine the origin and destination zone node IDs.
	// Parameters:
	// - trace: vector of GPSPoint representing a single trace.
	// - grid: the MatchingGrid that contains the grid cells.
	// - zoneNodeCoords: mapping from zone node id to its GDPoint (x,y) coordinate.
	// - numPointsToAverage: number of points from the start/end to average.
	// - originNodeId & destinationNodeId: outputs.
	void determineOriginDestinationNodes(const std::vector<GPSPoint>& trace,
		const MatchingGrid& grid,
		const std::map<int, GDPoint>& zoneNodeCoords,
		int numPointsToAverage,
		int& originNodeId,
		int& destinationNodeId)
	{
		if (trace.empty()) {
			originNodeId = -1;
			destinationNodeId = -1;
			return;
		}

		// Average first numPointsToAverage GPS points for the origin.
		GDPoint avgOrigin = { 0, 0 };
		int countOrigin = std::min(numPointsToAverage, static_cast<int>(trace.size()));
		for (int i = 0; i < countOrigin; ++i) {
			avgOrigin.x += trace[i].pt.x;
			avgOrigin.y += trace[i].pt.y;
		}
		avgOrigin.x /= countOrigin;
		avgOrigin.y /= countOrigin;

		// Average last numPointsToAverage GPS points for the destination.
		GDPoint avgDest = { 0, 0 };
		int countDest = std::min(numPointsToAverage, static_cast<int>(trace.size()));
		for (int i = static_cast<int>(trace.size()) - countDest; i < static_cast<int>(trace.size()); ++i) {
			avgDest.x += trace[i].pt.x;
			avgDest.y += trace[i].pt.y;
		}
		avgDest.x /= countDest;
		avgDest.y /= countDest;

		// Retrieve the grid cell for the averaged origin and destination.
		const GridCell* originCell = grid.getCell(avgOrigin.x, avgOrigin.y);
		const GridCell* destCell = grid.getCell(avgDest.x, avgDest.y);

		// Determine the best matching zone node from the candidates in each cell.
		if (originCell && !originCell->OriginZoneNodeIndices.empty())
			originNodeId = determineZoneNode(originCell->OriginZoneNodeIndices, avgOrigin, zoneNodeCoords);
		else
			originNodeId = -1;

		if (destCell && !destCell->DestinationZoneNodeIndices.empty())
			destinationNodeId = determineZoneNode(destCell->DestinationZoneNodeIndices, avgDest, zoneNodeCoords);
		else
			destinationNodeId = -1;
	}


} // end namespace MapMatching



// -------------------------
// -------------------------
// Example main() demonstrating advanced grid-based matching using MMLink
// -------------------------
int mapmatchingAPI() {


	fopen_s(&summary_log_file, "summary_log_file.txt", "w");
	double* MainVolume, * SubVolume, * SDVolume, Lambda;
	int*** MDMinPathPredLink;

	read_settings_file();
	read_mode_type_file();
	fopen_s(&logfile, "TAP_log.csv", "w");  // Open the log file for writing.
	no_nodes = get_number_of_nodes_from_node_file(no_zones, FirstThruNode);
	number_of_links = get_number_of_links_from_link_file();

	printf("# of nodes= %d, largest zone id = %d, First Through Node (Seq No) = %d, number of links = %d\n", no_nodes, no_zones,
		FirstThruNode, number_of_links);

	fprintf(summary_log_file, "no_nodes= %d, no_zones = %d, FirstThruNode (seq No) = %d, number_of_links = %d\n", no_nodes, no_zones,
		FirstThruNode, number_of_links);


	double system_wide_travel_time = 0;
	double system_least_travel_time = 0;

	//fprintf(logfile,
	//    "iteration_no,link_id,internal_from_node_id,internal_to_node_id,volume,capacity,voc,"
	//    "fftt,travel_time,delay\n");



	//Init(number_of_modes, no_zones);

	InitLinks();

	MDMinPathPredLink = (int***)Alloc_3D(number_of_modes, no_zones, no_nodes, sizeof(int)); 

	double** CostTo;

	CostTo = (double**)Alloc_2D(no_zones, no_nodes, sizeof(double));
	using namespace MapMatching;

	// Create some MMLinks.
	std::vector<MMLink> mmLinks;

	for (int k = 1; k <= number_of_links; k++)
	{
		double from_x, from_y, to_x, to_y; 
		from_x = g_node_vector[Link[k].internal_from_node_id].x;
		from_y = g_node_vector[Link[k].internal_to_node_id].y;
		to_x = g_node_vector[Link[k].internal_from_node_id].x;
		to_y = g_node_vector[Link[k].internal_to_node_id].y;

		mmLinks.push_back(MMLink({ from_x, from_y }, { to_x, to_y}, Link[k].internal_from_node_id, Link[k].internal_to_node_id));
	}



	// Compute overall boundaries from all MMLink endpoints.
	double left = 1e9, right = -1e9, bottom = 1e9, top = -1e9;
	for (const auto& L : mmLinks) {
		left = std::min({ left, L.fromPt.x, L.toPt.x });
		right = std::max({ right, L.fromPt.x, L.toPt.x });
		bottom = std::min({ bottom, L.fromPt.y, L.toPt.y });
		top = std::max({ top, L.fromPt.y, L.toPt.y });
	}

	// Build the matching grid.
	MatchingGrid mGrid(left, right, bottom, top, DEFAULT_GRID_RESOLUTION, FirstThruNode);
	mGrid.printSummary();
	for (size_t i = 0; i < mmLinks.size(); i++) {
		mGrid.insertLink(static_cast<int>(i), mmLinks[i].fromPt, mmLinks[i].toPt, mmLinks[i].external_from_node_id, mmLinks[i].external_to_node_id);
	}
	std::unordered_map<std::string, std::vector<GPSPoint>> gpsTraces; 
	std::unordered_map<std::string, int > gpsTraces_originNodeId;
	std::unordered_map<std::string, int > gpsTraces_destinationNodeId;
	std::unordered_map<std::string, int > gpsTraces_RouteId;
	readGPSTraceFile(gpsTraces);

	std::map<int, GDPoint> zoneNodeCoords;

	for (int i = 0; i < FirstThruNode; i++)  // TAZ centriod only 
	{
		GDPoint pt;
		pt.x = g_node_vector[i].x;
		pt.y = g_node_vector[i].y;
		zoneNodeCoords[g_node_vector[i].node_id] = pt;
	}

	std::vector<std::vector<std::vector<int>>> ODRouteIndices(
		no_zones + 1,
		std::vector<std::vector<int>>(no_zones + 1)
	);
	// ------------------------
	// Insert GPS points into the grid.
	// ------------------------
	// Loop over each agent  for that agent.
	int 	max_route_size = 1;

	for (const auto& agentPair : gpsTraces) {

		mGrid.cleanGPSPoint(); 
		const std::string& agentId = agentPair.first;

		std::vector<GPSPoint> gpsPoints;
		gpsPoints = agentPair.second;
		// Optionally, you might want to print or log the agent and trace information:
		// std::cout << "Processing agent " << agentId << ", trace " << traceNo << std::endl;
		for (const auto& gps : gpsPoints) {
			mGrid.insertGPSPoint(gps, gps.pt);
		}

		////#pragma omp parallel for
//		int Orig = agentPair.second.;  // get origin zone id
		int numPointsToAverage = 3;
		int  originNodeId = 0;
		int destinationNodeId = 0;

		determineOriginDestinationNodes(gpsPoints,
			mGrid,
			zoneNodeCoords,
			numPointsToAverage,
			originNodeId,
			destinationNodeId);
		gpsTraces_originNodeId[agentId] = originNodeId;
		gpsTraces_destinationNodeId[agentId] = destinationNodeId;
		gpsTraces_RouteId[agentId] = ODRouteIndices[originNodeId][destinationNodeId].size();
		ODRouteIndices[originNodeId][destinationNodeId].push_back(1); 

		if (ODRouteIndices[originNodeId][destinationNodeId].size() > max_route_size)
			max_route_size = ODRouteIndices[originNodeId][destinationNodeId].size(); 

	}

	InitializeLinkIndices(number_of_modes, no_zones, max_route_size);;


	for (const auto& agentPair : gpsTraces) {

		mGrid.cleanGPSPoint();
		const std::string& agentId = agentPair.first;

		std::vector<GPSPoint> gpsPoints;
		gpsPoints = agentPair.second;
		// Optionally, you might want to print or log the agent and trace information:
		// std::cout << "Processing agent " << agentId << ", trace " << traceNo << std::endl;
		for (const auto& gps : gpsPoints) {
			mGrid.insertGPSPoint(gps, gps.pt);
		}
		// ------------------------
		// Prepare arrays for each MMLink’s cost and matching trace number.
		// ------------------------
		std::vector<double> linkGeneralCost(mmLinks.size(), DEFAULT_GRID_RESOLUTION);
		std::vector<int>    linkMatchingTraceNo(mmLinks.size(), -1);

		// Process the grid cells.
		mGrid.processCells(mmLinks, linkGeneralCost, linkMatchingTraceNo);

		// Print the matching cost for each MMLink.
		for (size_t i = 0; i < mmLinks.size(); ++i) {
			std::printf("MMLink %zu: cost = %.3f, matched trace = %d\n",
				i, linkGeneralCost[i], linkMatchingTraceNo[i]);
		}


		if (gpsTraces_originNodeId[agentId] >= 1 && gpsTraces_destinationNodeId[agentId] >= 1)
		{
			int Orig = gpsTraces_originNodeId[agentId];
			int Dest = gpsTraces_destinationNodeId[agentId];
			std::vector<int> currentLinkSequence; // Temporary vector to store link indices


			int m = 1;
			Minpath(m, Orig, MDMinPathPredLink[m][Orig], CostTo[Orig]);
			int CurrentNode = g_map_external_node_id_2_node_seq_no[Dest];

			// MinPathPredLink is coded as internal node id 
			// 
			//double total_travel_time = 0;
			//double total_length = 0;
			//double total_FFTT = 0;
			// back trace 
			while (CurrentNode != Orig)
			{
				int k = MDMinPathPredLink[m][Orig][CurrentNode];

				if (k <= 0 || k > number_of_links || k == INVALID)
				{
					printf("A problem in All_or_Nothing_Assign() Invalid pred for node seq no %d Orig zone= %d \n\n", CurrentNode, Orig);
					break;
				}
				CurrentNode = Link[k].internal_from_node_id;

				if (CurrentNode <= 0 || CurrentNode > no_nodes)
				{
					printf("A problem in All_or_Nothing_Assign() Invalid node seq no %d Orig zone = %d \n\n", CurrentNode, Orig);
					break;
				}

				currentLinkSequence.push_back(k); // Store the link index
			}

			int route_id = gpsTraces_RouteId[agentId];
			AddLinkSequence(m, Orig, Dest, route_id, currentLinkSequence);

		}

	}
				std::vector<double> m_theta; 


	OutputRouteDetails("route_assignment.csv", m_theta);
	
	Free_3D((void***)MDMinPathPredLink, number_of_modes, no_zones, no_nodes); 
	Free_2D((void**)CostTo, no_zones, no_nodes);
	return 0;
}

void main()
{
	AssignmentAPI();


}


void DTA_AssignmentAPI() {
AssignmentAPI();

}

void DTA_SimulationAPI() {
SimulationAPI();
}
