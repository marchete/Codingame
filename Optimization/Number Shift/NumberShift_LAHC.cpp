/*
Starter Code for Number Shifting https://www.codingame.com/ide/puzzle/number-shifting

***This code is not for running on Codingame directly, but for using it locally, it works better with the submit.py script***
If you improve/bugfix it please send me a pull request or private pastebin.
I think the puzzle itself is the opponent, not other players. So if you have a better alternative I'd like to learn from it.

There are small parts omitted on the code, but simple enough to recreate it in 10 minutes.They are marked with a TODO:
The idea is not only Copy&Paste it, but learn from it, tweak and improve it. 

Based of on an alternative Hill Climbing like search:
//E.K.Burke and Y.Bykov, . "The Late Acceptance Hill-Climbing Heuristic".European Journal of Operational Research.
// https://pdfs.semanticscholar.org/28cf/19a305a3b02ba2f34497bebace9f74340ade.pdf?_ga=2.123170187.1062017964.1581936670-1790746444.1580199348

Basic Steps:
	0- Read initial data, run LAHC algorithm
	1- Create a new candidate solution, based on a previous accepted solution, changing little things (removing a number, truncating list of numbers, etc)
	2- Score it, based on some objective. I picked reducing both numbers in the grid and total sum of these numbers
	3- Based on the LAHC algorithm, accept it as a new accepted solution or discard it.
	4- Goto 1, Rinse and repeat until you have a real solution with 0 points and no numbers in the grid.

Visual Studio: Compile it as x64 (64 bits), no precompiled headers. Also increase stack size to 8Mb or you'll get segmentation faults on some functions at high levels.
GCC: g++-9 -std=c++14 -march=native -mfma -mavx2 -O3 -funroll-loops -fomit-frame-pointer -finline NumberShift_LAHC.cpp -o NumberShift_LAHC -pthread
CLANG: clang++-9 -std=c++14 -march=native -mfma -mavx2 -O3 -funroll-loops -fomit-frame-pointer -finline NumberShift_LAHC.cpp -o NumberShift_LAHC -pthread


Put the level in a file called 'level.txt', and the password level in a file called 'level_password.txt'
The executable has 2 input parameters
NumberShift_LAHC <LIMIT_TIME_IMPROVEMENT> <THREADS>
LIMIT_TIME_IMPROVEMENT Time in seconds for a LAHC reset, on higher maps >200 you'd need more than 60 seconds, on small maps 3-10s is enough.
THREADS: If compiled as multithreads, number of threads for LAHC. TODO: You need to add Threading code to enable it, and some #define.

example: ./NumberShift_LAHC 4 3 < level.txt
		   NumberShift_LAHC.exe 4 3 < level.txt
I tried Simulated Annealing, but for me LAHC is simpler and gives OK results.
*/

/*
//Recompile it to a bigger size once you reach the limit
const int MAX_W = 42;
const int MAX_H = 24;
const int MAX_NUMBERS = 1000;
*/
const int MAX_W = 17;
const int MAX_H = 10;
const int MAX_NUMBERS = 100;
//Input Parameter 1, 
long long LIMIT_TIME_IMPROVEMENT = 2 * 60 * 1000; //2 minutes 

#include <immintrin.h> //AVX,SSE Extensions
#include <atomic>
#include <algorithm>
#include <random>

#ifdef _MSC_VER
#include <bitset>
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#else
#include <bits/stdc++.h> //All main STD libraries
#endif

//#define MULTITHREAD
#ifdef MULTITHREAD
#include <thread> 
#include <mutex>
int THREADS = 4; //Input parameter 2
std::mutex mutexSOL; //Lock to block certain parts to a single Thread. Avoids race conditions.
#endif

//#define DEBG_MODE
#ifdef DEBG_MODE
#define ASSERT(x) assert(x)
#else 
#define ASSERT(x) 
#endif

typedef int32_t I;
using namespace std;

//Timer control
#define Now() chrono::high_resolution_clock::now()
struct Stopwatch {
	chrono::high_resolution_clock::time_point c_time, c_timeout;
	void Start(uint64_t us) { c_time = Now(); c_timeout = c_time + chrono::microseconds(us); }
	void setTimeout(uint64_t us) { c_timeout = c_time + chrono::microseconds(us); }
	inline bool Timeout() { return Now() > c_timeout; }
	long long EllapsedMilliseconds() { return chrono::duration_cast<chrono::milliseconds>(Now() - c_time).count(); }
} stopwatch;


string passwordLevel = "first_level";


//Custom random number generator, you can change it to whatever you like.
class Random {
public:
	const uint64_t K_m = 0x9b60933458e17d7d;
	const uint64_t K_a = 0xd737232eeccdf7ed;
	mt19937 E4;
	uint64_t seed;

	void setSeed(mt19937& e2) {
		uniform_int_distribution<int32_t> dist(numeric_limits<int32_t>::min(), numeric_limits<int32_t>::max());
		seed = dist(e2);
	}
	Random(int SEED) {
		std::random_device rd;
		E4 = mt19937(SEED);
		mt19937 e2(SEED);
		setSeed(e2);
	}
	Random() {
		std::random_device rd;
		std::seed_seq seedseq1{ rd(), rd(), rd() };
		E4 = std::mt19937(seedseq1);
		mt19937 e2(rd());
		setSeed(e2);
	}
	inline uint32_t xrandom() {
		seed = seed * K_m + K_a;
		return (uint32_t)(seed >> (29 - (seed >> 61)));
	}
	inline bool NextBool() {
		return (xrandom() & 4) == 4;
	}
	inline uint32_t NextInt(const uint32_t& range) {
		return xrandom() % range;
	}
	inline int32_t NextInt(const int32_t& a, const int32_t&  b) {
		return  (int32_t)NextInt((uint32_t)(b - a + 1)) + a;
	}
	inline float NextFloat() {
		uint32_t xr = xrandom();
		if (xr == 0U) return 0.0f;
		union
		{
			float f;
			uint32_t i;
		} pun = { (float)xr };
		pun.i -= 0x10000000U;
		return  pun.f;
	}
	inline float NextFloat(const float& a, const float& b) {
		return NextFloat()*(b - a) + a;
	}
};
Random rnd;

//Holds the MAP.totalNumbers, the initial number count
int COUNT_NUMBERS;

//Enumerater all numbers as a list of coordinates
struct Coord { uint8_t X; uint8_t Y; };
vector<Coord> NMB;
uint16_t IDX[MAX_W][MAX_H];

//WIDTH and HEIGHT
int W, H;
atomic<uint64_t> SimCount; //Atomic variables are useful on multithreading environments

//Some division caching, to avoid divisions on main loops
double INV_COUNT_NUMBERS = 1.0;
double INV_POINTS = 1.0;
double INV_TOTALNUMBERS = 1.0;

//Directions
const int U = 0;
const int D = 1;
const int L = 2;
const int R = 3;
const int DIR_X[] = { 0,0,-1,1 };
const int DIR_Y[] = { -1,1,0,0 };
const string strDIRS[] = { "U","D","L","R" };

//Fast bit count
#ifdef _MSC_VER
#define __builtin_popcountll _mm_popcnt_u64
#endif

//Set of numbers stored as bits, faster than vector<int> for many operations. Maybe a bitset was enough....
const int INT64_W = ((MAX_NUMBERS / 64) + (MAX_NUMBERS % 64 ? 1 : 0));
struct  NumberSet {
	static  NumberSet Mask;
	uint64_t W[INT64_W];

	NumberSet() { clear(); }
	NumberSet(vector<int> V) {
		clear();
		for (auto&v : V)
			set((size_t)v);
	}
	NumberSet(vector<size_t> V) {
		clear();
		for (auto&v : V)
			set(v);
	}
	inline void set(const size_t& N) {
		uint64_t element = N / 64;
		uint64_t pos = 63 - (N % 64);
		W[element] |= (1ULL << pos);
	}
	inline bool get(const int& N)const {
		uint64_t element = N / 64;
		uint64_t pos = 63 - (N % 64);
		return (W[element] & (1ULL << pos)) != 0;
	}
	void unset(const int& N) {
		uint64_t element = N / 64;
		uint64_t pos = 63 - (N % 64);
		W[element] &= ~(1ULL << pos);
	}

	void negate() {
		for (int i = 0; i < INT64_W; ++i)
		{
			W[i] = ~W[i];
		}
		this->_and(Mask);
	}

	void clear() {
		for (int i = 0; i < INT64_W; ++i)
		{
			W[i] = 0;
		}
	}
	void setones() {
		*this = Mask;
	}

	inline int count() const {
		int result = 0;

		for (int i = 0; i < INT64_W; ++i)
		{
			result += (int)__builtin_popcountll(W[i]);
		}
		return result;
	}

	bool Intersects(const NumberSet& B)
	{
		for (int i = 0; i < INT64_W; ++i)
		{
			if ((W[i] & B.W[i]) != 0)
				return true;
		}
		return false;
	}
	bool Contains(int N) {
		uint64_t element = N / 64;
		uint64_t pos = 63 - (N % 64);
		return (((W[element] >> pos) & 1) == 1);
	}


	void _and(const NumberSet& b) {
		for (int IIN = 0; IIN < INT64_W; ++IIN)
		{
			W[IIN] &= b.W[IIN];
		}
	}

	void _or(const NumberSet& b) {
		for (int IIN = 0; IIN < INT64_W; ++IIN)
		{
			W[IIN] |= b.W[IIN];
		}
	}

	int CountIntersects(const NumberSet& B)
	{
		int count = 0;
		for (int i = 0; i < INT64_W; ++i)
		{
			count += (int)__builtin_popcountll(W[i] & B.W[i]);
		}
		return count;
	}
	inline bool Disjoint(const NumberSet& B) { return CountIntersects(B) == 0; }
	bool isFullyContainedIn(const NumberSet& B)const {
		for (int u = 0; u < INT64_W; ++u)
		{
			if ((W[u] & B.W[u]) != W[u])
				return false;
		}
		return true;
	}
	static NumberSet CreateMask(int CountNumbers) {
		NumberSet n;
		for (size_t i = 0; i < CountNumbers; ++i)
			n.set(i); //Ugly but it works...
		return n;
	}
};

NumberSet NumberSet::Mask;


ostream& operator<<(ostream& os, const NumberSet& m)
{
	for (int i = 0; i < INT64_W; ++i)
	{
		os << bitset<64>(m.W[i]);
	}
	return os;
};

//Simple Fisher Yates shuffling, minimal code without external references.
template <class T>
void do_shuffle(T& smallDLX)
{
	for (int i = (int)smallDLX.size() - 1; i > 0; --i) {
		int r = rnd.NextInt(i + 1);
		if (i != r) {
			swap(smallDLX[i], smallDLX[r]);
		}
	}
}


struct Move {
	uint32_t srcIDX;
	uint32_t destIDX;
	uint32_t dir;
	uint32_t sign;
	//I keep src and dest values to check coherences, if some move to apply no longer has the same src and dest val I won't apply it
	uint32_t srcVal;
	uint32_t destVal;
	Move() {}

	Move(int _srcID, int _destID, int _dir, int _sign, int _srcVal, int _destVal) {
		srcIDX = _srcID;
		destIDX = _destID;
		dir = _dir;
		sign = _sign;
		srcVal = _srcVal;
		destVal = _destVal;
	}
};
ostream& operator<<(ostream& os, const Move& m)
{
	os << (int)NMB[m.srcIDX].X << ' ' << (int)NMB[m.srcIDX].Y << ' ' << strDIRS[m.dir] << " " << (m.sign <= 0 ? "-" : "+");
	return os;
};

//There is probably a better way, I don't care
inline void Add(vector<Move>&A, const vector<Move>& B)
{
	for (auto&b : B)
		A.push_back(b);
}

struct Grid;
//Strategy consists in a list of Moves.
struct  Strategy {
	NumberSet  UN; //Keeps a list of used moves, for fast checking some things
	uint8_t Endpoint = 0; //If 1 it's a zero-sum set, it ends in a X = X, so it's a zero.
	vector<Move> linear;
	inline void addMove(const Move& m) { linear.push_back(m); UN.set(m.srcIDX); }
	inline void resize(const int& N) { linear.resize(N); }
	inline void addPlan(const Strategy& S) {
		Add(linear, S.linear);
		UN._or(S.UN);
	}
	inline void addAsOptional(const Strategy& S, uint32_t linkidx, uint32_t linkvalue) {
		//Optionals? parts of the Strategy that src doesn't change the value of dest.
		Add(linear, S.linear);
		UN._or(S.UN);
	}
	inline void setEndPoint(int NumberIDX) { UN.set(NumberIDX); Endpoint = 1; }
	inline void clear() { UN.clear(); linear.clear(), Endpoint = 0; }
	~Strategy() { linear.clear(); }

	void ApplyMoves(Grid& g, bool dontPlayEndpoint, vector<int>& UnusedNumbers);
};

//Here goes the real solution.
atomic<bool> solved; //Atomic because of multithreading
Strategy SolvedArray;

//Holds the map of numbers
struct Grid {
	uint16_t G[MAX_W][MAX_H] = { 0 }; //The grid
	Strategy Plan[MAX_W][MAX_H];  //For tracking the list of Moves that leads to that state. When I do a move I transfer the strategy on the source number to the destination number
	int totalPoints = 0; //Remaining sum of numbers.For scoring purposes
	int totalNumbers = 0; //Remaining numbers. For scoring purposes
	 //There are probably more ways to score the game state

	inline uint32_t ApplyMove(const Move& m)
	{
		//Get source point
		auto srcX = NMB[m.srcIDX].X;
		auto srcY = NMB[m.srcIDX].Y;
		auto srcScore = G[srcX][srcY];

		if (m.srcVal != srcScore) return -1; //incoherent

		//Get dest point
		auto destX = srcX + DIR_X[m.dir] * srcScore;
		auto destY = srcY + DIR_Y[m.dir] * srcScore;
		auto destScore = G[destX][destY];
		auto destID = IDX[destX][destY];

		if (m.destIDX != destID || destScore != m.destVal) return -1; //Incoherent

		//Subtract current values from tracking variables.
		--totalNumbers;
		totalPoints -= destScore;
		totalPoints -= srcScore;
		//Apply the move
		G[srcX][srcY] = 0;
		auto Dest = (m.sign <= 0 ? abs(destScore - srcScore) : destScore + srcScore);
		G[destX][destY] = Dest;

		//Add the new value to tracking variables.
		totalPoints += Dest;
		if (Dest == 0)
		{
			--totalNumbers;
		}
		++SimCount;

		//Transfer the plan from source point to destination point.
		/*		if (2 * destScore == srcScore)
				{
					Plan[srcX][srcY].addMove(m);
					//Sppliting movelists in optional parts
					Plan[destX][destY].addAsOptional(Plan[srcX][srcY], destID, destScore);
					Plan[srcX][srcY].clear();
				}
				else {*/
		Plan[srcX][srcY].addMove(m);
		Plan[destX][destY].addPlan(Plan[srcX][srcY]);
		Plan[srcX][srcY].clear();
		if (Dest == 0) //Ends with a zero, it's a zero-sum set
		{
			Plan[destX][destY].setEndPoint(destID);
		}
		//}
		return destID;
	}

	//This function tries to sub a number when it doesn't change the destination value:
	// It's the case:  2 * desVal == srcVal , like  srcVal = 4 and destVal = 2
	inline bool doMerge(const int& srcID, const int& destID) {
		//Get source point
		auto srcX = NMB[srcID].X;
		auto srcY = NMB[srcID].Y;
		auto srcVal = G[srcX][srcY];
		if (srcVal > W || srcVal == 0) return false; //Incoherent
		//Get destination point
		auto targetX = NMB[destID].X;
		auto targetY = NMB[destID].Y;
		auto targetVAL = G[targetX][targetY];
		for (int d = 0; d < 4; ++d)
		{
			auto destX = srcX + DIR_X[d] * srcVal;
			if (destX != targetX) continue;//Incoherent
			auto destY = srcY + DIR_Y[d] * srcVal;
			if (destY != targetY) continue;//Incoherent
			if (2 * G[destX][destY] != srcVal) continue; //Can't merge this move

			//Succesful merge
			Move m = Move(srcID, destID, d, 0, srcVal, targetVAL);
			ApplyMove(m);
			ASSERT(targetVAL == G[targetX][targetY]);

			return true;
		}
		//Can't merge anything
		return false;
	}

	//Generates all possible moves for a given Number ID
	inline void ExplodeMoves(const int& srcID, vector<Move>& moves)
	{
		//Get source Point
		auto srcX = NMB[srcID].X;
		auto srcY = NMB[srcID].Y;
		auto srcVal = G[srcX][srcY];
		if (srcVal > W || srcVal == 0) return; // Invalid src or out of bounds

		//Try to move in all 4 directions
		int allowSum = rnd.NextInt(100);
		for (int d = 0; d < 4; ++d)
		{
			auto destX = srcX + DIR_X[d] * srcVal;
			if (destX < 0 || destX >= W) continue; //Out of bounds
			auto destY = srcY + DIR_Y[d] * srcVal;
			if (destY < 0 || destY >= H) continue; //Out of bounds
			auto destID = IDX[destX][destY];
			auto destVal = G[destX][destY];
			if (destVal == 0) continue;  //Destination == 0, invalid

			//Subtract
			Move m = Move(srcID, destID, d, 0, srcVal, destVal);
			moves.push_back(m);
			//Add, Only 20% of the times because it's less frequent
			if (allowSum < 20)
			{
				m.sign = 1;
				moves.push_back(m);
			}
		}
		return;
	}

	inline bool doRandomMove() {
		//Pick a random number as starting point of the loop
		int randomNumber = rnd.NextInt(COUNT_NUMBERS);
		vector<Move> moves;
		for (int NN = 0; NN < COUNT_NUMBERS; ++NN)
		{
			++randomNumber;
			if (randomNumber >= COUNT_NUMBERS)
				randomNumber = 0;

			//Try to get some valid move
			ExplodeMoves(randomNumber, moves);
			//If there are moves for that point, use a random one
			if (moves.size() > 0)
			{
				ApplyMove(moves[rnd.NextInt((uint32_t)moves.size())]);
				return true;
			}
		}
		//No more valid moves
		return false;
	}

};
Grid MAP;

//Apply moves from and also try to merge unused numbers
void Strategy::ApplyMoves(Grid& g, bool dontPlayEndpoint, vector<int>& UnusedNumbers) {
	for (auto&m : linear)
	{
		//Try to add optionals as srcID
		int mergeIDX = m.srcIDX;

		if (UnusedNumbers.size() > 0 && rnd.NextInt(1000) < 800)
			for (auto& unused : UnusedNumbers)
			{
				g.doMerge(unused, m.srcIDX); //Try to merge unused numbers before the move
			}
		if (m.srcVal == m.destVal && dontPlayEndpoint)
			continue;
		g.ApplyMove(m);
		//Last move merge
		if ((&m == &linear[linear.size() - 1]) && UnusedNumbers.size() > 0 && rnd.NextInt(1000) < 800)
			for (auto& unused : UnusedNumbers)
			{
				g.doMerge(unused, m.destIDX); //Try to merge unused numbers after the move
			}
		mergeIDX = m.destIDX;
	}
}

//Structure for LAHC search, it's a list of Strategies + Score of it.
struct  LAHC_Node {
	NumberSet usedNumbers;
	NumberSet completeNumbers;
	double Score = 0.0;
	double A, B, C, D; //keeps different parts of the Score, for debugging purposes.
	Grid grid;
	vector<Strategy> Movelists;

	int MutateType = 0; //Just for tracing stats of what type of mutation works best

	//Generate scores
	void CalcScore(int _MutateType) {
		MutateType = _MutateType;
		Movelists.clear();
		usedNumbers.clear();
		completeNumbers.clear();
		for (auto& n : NMB)
		{
			auto x = n.X;
			auto y = n.Y;
			if (grid.Plan[x][y].linear.size() > 0)
			{
				Movelists.push_back(grid.Plan[x][y]);
				int Index = (int)Movelists.size() - 1;
				auto& plan = Movelists[Index];
				plan.UN.set(IDX[x][y]); //TODO only incomplete numbers?
				usedNumbers._or(plan.UN);
				if (plan.Endpoint > 0) //Zero-sum sets
				{
					completeNumbers._or(plan.UN);
				}
			}
		}
		int remainingNumbers = MAP.totalNumbers - grid.totalNumbers;
		int completeNumberCount = (int)completeNumbers.count();
		int remainingPoints = MAP.totalPoints - grid.totalPoints;

		//Premises -> Use more numbers, reduce total points, and reduce squared total points.
		A = -1.0;
		B = 0.0;
		C = 0.0;
		D = 0.0;

		/*
		A = K_A*percent of remaining numbers; //K_A can be any number
		B = K_B*percent of complete number count; //K_B much smaller than K_A
		C = K_C*percent of remaining points; //Similar order than K_A
		D = 0.0; //more ideas?
		*/
		Score = A + B + C + D;
		if (Score == -1.0)
		{
			cerr << "TODO: Create a proper scoring for the algorithm. Check around lines: " << __LINE__ << endl;
			abort();
		}

	}
	LAHC_Node() {}
	void clear() {
		grid = MAP; //Copy the original map.
		Movelists.clear();
		usedNumbers.clear();
	}
};


//This mutator randomly remove moves from the lists. Any further move that is incoherent won't be applied. Then make random moves.
inline void Mutate_Points(LAHC_Node& currentBest, LAHC_Node& candidate)
{
	candidate.clear();
	vector<Move> linearMoves;
	vector<int> Z;
	NumberSet UsedNumbers;

	//Get a list of strategies and used moves in them
	for (auto z = 0; z < currentBest.Movelists.size(); ++z)
		if (rnd.NextInt(1000) < 990) //Some chance to ignore an Strategy
		{
			Z.push_back(z);
			for (auto&l : currentBest.Movelists[z].linear)
			{
				UsedNumbers.set(l.srcIDX);
				if (currentBest.Movelists[z].Endpoint > 0)
					UsedNumbers.set(l.destIDX);
			}
		}
	//Shuffle the order of Strategies
	do_shuffle<vector<int>>(Z);
	for (auto&k : Z)
		Add(linearMoves, currentBest.Movelists[k].linear);

	if (linearMoves.size() > 0)
	{

		bool tryMerge = /*currentBest.Movelists.size() > 10 &&*/ (currentBest.usedNumbers.count() > COUNT_NUMBERS * 50 / 100) && rnd.NextInt(100) < 80;
		vector<int> UnusedNumbers;
		//Try merging unused numbers to current Strategies
		if (tryMerge)
		{
			for (int i = 0; i < COUNT_NUMBERS; ++i)
			{
				if (!UsedNumbers.get(i))
				{
					UnusedNumbers.push_back(i);
				}
			}
		}
		//Select random Moves, between 1 and 4, to "delete" them
		int countRemove = rnd.NextInt(1, min(4, (int)linearMoves.size()));
		unordered_set<int> toRemove;
		for (int i = 0; i < countRemove; ++i)
		{
			int mutateIndex;
			do {
				//Random index
				mutateIndex = (int)rnd.NextInt((uint32_t)linearMoves.size());
			} while (toRemove.find(mutateIndex) != toRemove.end()); //Ensure it's not repeated
			toRemove.insert(mutateIndex);
		}
		//Apply all moves except the deleted ones
		for (int i = 0; i < linearMoves.size(); ++i)
			if (toRemove.find(i) == toRemove.end()) //Except if they are "deleted"
			{
				//Chance to merge
				bool chanceMerge = tryMerge && (rnd.NextInt(1000) < 900);
				//Some chance to merge at source number
				if (chanceMerge)
				{
					for (auto& unused : UnusedNumbers)
					{
						candidate.grid.doMerge(unused, linearMoves[i].srcIDX);
					}
				}
				candidate.grid.ApplyMove(linearMoves[i]);
				//Some chance to merge at the destination number
				if (chanceMerge)
				{
					for (auto& unused : UnusedNumbers)
					{
						candidate.grid.doMerge(unused, linearMoves[i].destIDX);
					}
				}

			}
	}

	//Now a normal MC;
	while (candidate.grid.doRandomMove()) {}
	candidate.CalcScore(1);
}

//This mutator randomly truncates some lists of numbers. 
//I.e. a list with 20 moves can be truncated at the end to a shorter length, to just 14 moves, then do random moves again.
inline void Mutate_Lists(LAHC_Node& currentBest, LAHC_Node& candidate)
{
	candidate.clear();
	if (currentBest.Movelists.size() > 0)
	{
		unordered_set<int> usedPlan;
		bool tryMerge = currentBest.Movelists.size() > 10 && (currentBest.usedNumbers.count() > COUNT_NUMBERS * 50 / 100) && rnd.NextInt(100) < 80;
		vector<int> UnusedNumbers;
		if (tryMerge)
		{
			for (int i = 0; i < COUNT_NUMBERS; ++i)
			{
				if (!currentBest.usedNumbers.get(i))
				{
					UnusedNumbers.push_back(i);
				}
			}
		}

		//Pick between 1 and 3 strategies at random, we will mutate these strategies
		int ListsToMutate = rnd.NextInt(1, min(3, (int)currentBest.Movelists.size()));
		for (int i = 0; i < ListsToMutate; ++i)
		{
			int mutateIndex;
			do {
				mutateIndex = (int)rnd.NextInt((uint32_t)currentBest.Movelists.size());
				//Unique indexes
			} while (usedPlan.find(mutateIndex) != usedPlan.end());
			usedPlan.insert(mutateIndex);
		}

		int i = rnd.NextInt((int)currentBest.Movelists.size());

		for (auto& bbg : currentBest.Movelists)
		{
			++i;
			if (i >= currentBest.Movelists.size())
			{
				i = 0;
			}
			uint32_t limitCopy = (uint32_t)currentBest.Movelists[i].linear.size();
			if (usedPlan.find(i) != usedPlan.end())
			{ //To mutate, we'll cut these
				limitCopy = (int)rnd.NextInt(limitCopy);// max(1, (int)rnd.NextInt(limitCopy));
			}
			for (uint32_t j = 0; j < limitCopy; ++j)
			{
				bool chanceMerge = tryMerge && (rnd.NextInt(1000) < 800);
				//Some chance to merge at source Number
				if (chanceMerge)
				{
					for (auto& unused : UnusedNumbers)
					{
						candidate.grid.doMerge(unused, currentBest.Movelists[i].linear[j].srcIDX);
					}
				}
				auto destIDX = candidate.grid.ApplyMove(currentBest.Movelists[i].linear[j]);
				//Some chance to merge at source Number
				if (chanceMerge)
				{
					for (auto& unused : UnusedNumbers)
					{
						candidate.grid.doMerge(unused, currentBest.Movelists[i].linear[j].destIDX);
					}
				}

			}
		}
	}
	//Now a normal MC.
	while (candidate.grid.doRandomMove()) {}

	candidate.CalcScore(2);

}

//From a given solution construct a new candidate solution
inline void Mutate(LAHC_Node& lastAccepted, LAHC_Node& candidate)
{
	int r = rnd.NextInt(100);
	if (r < 80) Mutate_Points(lastAccepted, candidate);
	else Mutate_Lists(lastAccepted, candidate);
	//Additional improvements? Add better mutators
}

//E.K.Burke and Y.Bykov, . "The Late Acceptance Hill-Climbing Heuristic".European Journal of Operational Research. 
// https://pdfs.semanticscholar.org/28cf/19a305a3b02ba2f34497bebace9f74340ade.pdf?_ga=2.123170187.1062017964.1581936670-1790746444.1580199348
//On bigger maps maybe you need higher Lfa sizes
//Small Lfa converges faster, but returns worse solutions, increase it in higher levels
//I also increased it in 
const int INITIAL_LFA_SIZE = 600;
const int LFA_SIZE_95 = 2000; //Increased LFA on ending search

void Worker_LAHC(int ID)
{
	//LAHC Variables, like on the Paper cited above
	int Lfa = INITIAL_LFA_SIZE; //Fitness vector size
	vector<double> Fitness; //Part of the LAHC search algorithm
	uint64_t v = 0;
	uint64_t I = 0; //Paper:First iteration I=0;
	LAHC_Node lastAccepted; //This holds the last Accepted Solution, can be worse than the Best found so far.
	LAHC_Node candidate = lastAccepted; //candidate is the temporary solution, derived from the last Accepted via mutations (=small changes)

	//Custom variables
	LAHC_Node bestStrategy; //The best found until now. This is not in the original Paper but I use it to force a reset after X seconds without a real improvement.
	long long localTimeLimit = LIMIT_TIME_IMPROVEMENT;
	Stopwatch lastImprovement; //This timer is to force a full reset if we got stuck on a local maximum.
	lastImprovement.Start(0);

	//Paper:Produce an initial solution s
	lastAccepted.clear();
	Mutate(lastAccepted, candidate);  //Generate a random movelist, it will be far from the real solution but it's just an starting point
	lastAccepted = candidate;  //Right now all is this candidate
	bestStrategy = candidate;
	Fitness.resize(Lfa);
	for (int k = 0; k < Lfa; ++k)
	{
		Fitness[k] = lastAccepted.Score; //Paper:For all k € {0...Lfa-1} fk:=C(s)
	}
	cerr << "LAHC " << ID << " Lfa size:" << Lfa << endl;
	//Paper:Do until a chosen stopping condition
	while (!solved)
	{
		Mutate(lastAccepted, candidate); //Paper:Construct a candidate solution s*  and Calculate its cost function C(s*)
		v = I % Lfa; //v := I mod Lfa

		//Paper:If C(s*)<=fv or C(s*)<=C(s)
		if ((candidate.Score >= lastAccepted.Score || candidate.Score >= Fitness[v]))
		{ //Paper:Then accept the candidate s:=s*

			if (candidate.grid.totalPoints == 0) //Stopping condition
			{ //Solved!!!
				if (!solved)
				{
#ifdef MULTITHREAD
					mutexSOL.lock();
#endif
					solved = true;
					SolvedArray.clear();
					for (auto& c : candidate.Movelists)
						SolvedArray.addPlan(c);
					cerr << "--- SOLUTION --- has " << candidate.Movelists.size() << " GROUPS. SimCount:" << SimCount << " Took " << stopwatch.EllapsedMilliseconds() << "ms" << endl;
					cerr << passwordLevel << endl;
					for (auto&m : SolvedArray.linear)
					{
						//		cerr << m << endl;
						cout << m << endl;
					}
#ifdef MULTITHREAD
					mutexSOL.unlock();
#endif
				}
				return; //We have found a solution, exit the program
			}

			//Keep a best candidate too, not only the last Accepted
			if (candidate.Score > bestStrategy.Score)
			{
				bestStrategy = candidate;
				lastImprovement.Start(0); //restart the last improvement timer.
			}
			lastAccepted = candidate;
		}
		Fitness[v] = lastAccepted.Score; //Paper:Insert the current cost into the fitness array fv:=C(s)

		++I;//Paper:Increment the iteration number I : = I + 1

		if (I % 10000 == 0) //Convergence check, if too much time has passed without a best Solution restart the search.
		{
			//Some percentages about points & numbers used.
			double points = 100.0*(double)(MAP.totalPoints - bestStrategy.grid.totalPoints) / (double)MAP.totalPoints;
			double coverage = 100.0*(double)(COUNT_NUMBERS - bestStrategy.grid.totalNumbers)*INV_COUNT_NUMBERS;

			if (Lfa == INITIAL_LFA_SIZE && (points > 95 || coverage > 95))
			{ //Increase Lfa for ending search
				Lfa = LFA_SIZE_95;
				Fitness.resize(Lfa);
				for (int i = INITIAL_LFA_SIZE; i < Lfa; ++i)
				{
					Fitness[i] = Fitness[i % INITIAL_LFA_SIZE];
				}
			}

			//If after some time we don't have a new best Solution, just start over with a bigger Localtimelimit
			if (lastImprovement.EllapsedMilliseconds() > localTimeLimit)
			{
				lastImprovement.Start(0);
				cerr << "Reset LAHC on worker " << ID << " LAHC Size:" << Lfa << " Limit Time Improvement:" << localTimeLimit / 1000 << "s" << endl;

				//I prefer to add some more time to the next attempt on big maps
				if (localTimeLimit < 3 * 60 * 1000)
				{
					if (COUNT_NUMBERS > 150)
						localTimeLimit += 5000; //5 secs increase
					else localTimeLimit += 500; //500 ms increase
				}
				//Produce an initial solution s
				lastAccepted.clear();
				Mutate(lastAccepted, candidate);
				lastAccepted = candidate;
				bestStrategy = candidate;
				Lfa = INITIAL_LFA_SIZE; //Low Lfa converges faster, increase it in higher levels
				Fitness.resize(Lfa);
				for (int k = 0; k < Lfa; ++k)
				{
					Fitness[k] = lastAccepted.Score; //For all k € {0...Lfa-1} fk:=C(s)
				}

			}

		}

		if ((I % 40000) == 0) //Each 40k iterations notify on screen
		{
			auto timer = lastImprovement.EllapsedMilliseconds();
			auto secs = timer / 1000;
			auto ms = timer % 1000;
			cerr << "ID:" << ID << " Best Score:" << (int)bestStrategy.Score;
			cerr << " Points:" << bestStrategy.grid.totalPoints << "=" << (double)(MAP.totalPoints - bestStrategy.grid.totalPoints)*100.0 / (double)MAP.totalPoints;
			cerr << "% Remaining Numbers:" << (bestStrategy.grid.totalNumbers) << " T:" << secs << "s " << ms << "ms";
			cerr << endl;
		}
	}
}


void ParallelWork() {

#ifdef MULTITHREAD
	vector<thread> threads(THREADS);
	//TODO TODO TODO: Create threads, it won't work without them.........
	cerr << "TODO TODO TODO : Create threads, it won't work without them........." << endl;
	//TODO: Create threads here.....
	//Waiting loop for a solution of any of the threads.
	int C = 0;
	while (!solved)
	{
		if (++C > 300)
		{
			cerr << "SimCount:" << SimCount / 1000000ULL << "M Time:" << (stopwatch.EllapsedMilliseconds() / 1000) << " Points:" << MAP.totalPoints << " Numbers:" << MAP.totalNumbers;
			cerr << endl;
			C = 0;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); //Notify each 30 seconds. but in smaller steps to be more responsive to a solved solution.
	}
#else 
	Worker_LAHC(0);
#endif

#ifdef MULTITHREAD
	cerr << "Waiting for threads to end...." << endl;
	for (auto&t : threads) //who cares? I win!
	{
		t.join();
	}
#endif

}

#ifdef _MSC_VER
void setStack() {}
#else
#include <sys/resource.h>
void setStack()
{
	const rlim_t kStackSize = 16 * 1024 * 1024;   // min stack size = 16 MB
	struct rlimit rl;
	int result;

	result = getrlimit(RLIMIT_STACK, &rl);
	if (result == 0)
	{
		if (rl.rlim_cur < kStackSize)
		{
			rl.rlim_cur = kStackSize;
			result = setrlimit(RLIMIT_STACK, &rl);
			if (result != 0)
			{
				fprintf(stderr, "setrlimit returned result = %d\n", result);
			}
		}
	}
}
#endif

int main(int argc, char *argv[])
{
	//The program uses a lot of stack variables, it needs to be increased
	setStack();
	srand(unsigned(time(0)));
	stopwatch.Start(0);
	SimCount = 0;
	solved = false;

	if (argc > 1) {
		LIMIT_TIME_IMPROVEMENT = atoi(argv[1]) * 1000;
		cerr << "LIMIT TIME IMPROVEMENT IS " << LIMIT_TIME_IMPROVEMENT << endl;
	}

	if (argc > 2) {
		int n = max(1, atoi(argv[2]));
#ifdef MULTITHREAD
		THREADS = n;
		cerr << "THREADS" << THREADS << endl;
#else 
		if (n > 1)
			cerr << "Parameter THREAD=" << n << " ignored, not compiled as multi " << endl;
#endif
}


	//Try to open a file with the level password
	ifstream f("level_password.txt");
	if (f.good()) { std::getline(f, passwordLevel); f.close(); }

	//if level.txt exists use it instead of cin
	/*if (file_exists("level.txt"))
	{
		auto r = freopen("level.txt", "r", stdin);
		//r = freopen("solution.txt", "w", stdout);
	}*/


	cerr << "***********LEVEL:" << passwordLevel << endl;
	cin >> W >> H; cin.ignore();
	if (W > MAX_W || H > MAX_H)
	{
		cerr << "Width overflow, recompile with a bigger MAX_W and MAX_H. MAX_W>=" << W << "; MAX_H>= " << H << ";" << endl;
		assert(W <= MAX_W);
		assert(H <= MAX_H);
	}

	//Load the Map
	MAP.totalPoints = 0;
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			cin >> MAP.G[x][y]; cin.ignore();
			if (MAP.G[x][y] != 0)
			{
				++MAP.totalNumbers;
				MAP.totalPoints += MAP.G[x][y];
				IDX[x][y] = (uint16_t)MAP.totalNumbers;
				NMB.push_back(Coord{ (uint8_t)x,(uint8_t)y });
			}
		}
	}

	COUNT_NUMBERS = MAP.totalNumbers;
	NumberSet::Mask = NumberSet::CreateMask(COUNT_NUMBERS);
	if (COUNT_NUMBERS >= MAX_NUMBERS)
	{
		cerr << "INCREASE MAX_NUMBERS!!!!" << COUNT_NUMBERS << " > " << MAX_NUMBERS << endl;
		assert(COUNT_NUMBERS < MAX_NUMBERS);
	}

	INV_COUNT_NUMBERS = 1.0 / (double)COUNT_NUMBERS;
	INV_POINTS = 1.0 / (double)MAP.totalPoints;

	ParallelWork();
	return 0;
}
