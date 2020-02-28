//Optimization pragmas for Codingame
#pragma GCC optimize("O3","unroll-loops","omit-frame-pointer","inline")
#pragma GCC option("arch=native","tune=native")
#pragma GCC target("sse,sse2,sse3,ssse3,sse4,popcnt,bmi2,abm,mmx,avx,avx2")
#include <bits/stdc++.h> //All main STD libraries
#include <immintrin.h> //AVX/SSE Extensions

using namespace std;
/*
Starter bot for Oware Abapa https://www.codingame.com/ide/puzzle/oware-abapa
Implements basic simulation + Negamax Search + Scoring
It works right out of the box. You can change eval in line 584+
it's the function float Game_Oware::Eval(int playerID, int enemyID)

You can also change the search algorithm for another thing (minimax, MCTS).

Simulation is pretty basic, not optimized.
*/



/****************************************  TIMER CODE **************************************************/
#include <chrono>
#define Now() chrono::high_resolution_clock::now()
struct Stopwatch {
	chrono::high_resolution_clock::time_point c_time, c_timeout;
	void Start() { c_time = Now(); }
	void setTimeout(int us) { c_timeout = c_time + chrono::microseconds(us); }
	inline bool Timeout() { return Now() > c_timeout; }
	long long EllapsedMicroseconds() { return chrono::duration_cast<chrono::microseconds>(Now() - c_time).count(); }
	long long EllapsedMilliseconds() { return chrono::duration_cast<chrono::milliseconds>(Now() - c_time).count(); }
}stopwatch;
//} Stopwatch


//#define DBG_MODE
#ifdef DBG_MODE
#include <cassert>
#define DBG(x) x
#else
#define DBG(x)
#endif

/****************************************  GLOBAL VARIABLES **************************************************/
int MY_ID = 0;
int SIMCOUNT = 0;

/****************************************  VIRTUAL GAME AND MOVE **************************************************/
class _Game;
class _Move;
typedef shared_ptr<_Move> Move;
typedef shared_ptr<_Game> Game;

class _Move : public std::enable_shared_from_this<_Move> {
public:
	virtual int GetType(const Game &S) { DBG(cerr << "GTYPE" << endl;); abort(); }
	virtual int GetMove(const Game &S) { DBG(cerr << "GMOVE" << endl;); abort(); }
	virtual std::string PrintMove(const Game &S) { DBG(cerr << "PMOVE" << endl;); abort(); }
	Move getptr() { return shared_from_this(); }
	virtual ~_Move() {}
};

class _Game : public std::enable_shared_from_this<_Game> {
public:
	int myGameID = 0; //Holds real ID from the game
	int turn = -1;  //turn from the game
	int simTurn = 0; //Simulated turn == depth
	int idToPlay;  //Current playing ID
	uint64_t Hash;
	Game getptr() { return shared_from_this(); }
	virtual inline void swap_turn()noexcept { idToPlay = 1 - idToPlay; }
	virtual int getTimeLimit() { if (turn == 0) return 500 * 1000; else  return 39 * 1000; }
	//virtual void GenerateAllPossibleMoves() { DBG(cerr<<"GENALL"<<endl;);abort();}
	virtual int getPlayerCount() { DBG(cerr << "PCOUNT" << endl;); abort(); }
	virtual int getunitsPerPlayer() { DBG(cerr << "UPER" << endl;); abort(); }
	virtual void readConfig(Stopwatch& t) { DBG(cerr << "RCONFIG" << endl;); abort(); }
	virtual void readTurn(Stopwatch& t) { DBG(cerr << "RTURN" << endl;); abort(); }
	virtual Move decodeMove(const int& idx) { DBG(cerr << "DECMOVE" << endl; ); abort(); }
	virtual void valid_moves(const int& id, vector<int> &Possible_Moves) { DBG(cerr << "VMOVES" << endl;); abort(); }

	virtual uint64_t CalcHash(const int& _NextMove)noexcept { DBG(cerr << "CalcHash" << endl; ); abort(); }
	virtual int getPackSize()noexcept { DBG(cerr << "getPack" << endl; ); abort(); }
	virtual void Pack(uint8_t* g)noexcept { DBG(cerr << "SERI" << endl;); abort(); }
	virtual void Unpack(uint8_t* g)noexcept { DBG(cerr << "DES" << endl;); abort(); }

	virtual bool isEndGame() { DBG(cerr << "ENDG" << endl; ); abort(); }
	virtual void Simulate(Move move) { DBG(cerr << "SIM" << endl;); abort(); }
	virtual void CopyFrom(Game original) { DBG(cerr << "CopyFrom" << endl;); abort(); }
	virtual void UnSimulate(Game g, Move move) { DBG(cerr << "USIM" << endl;); abort(); }
	virtual Game Clone() { DBG(cerr << "CLONE" << endl;); abort(); }
	virtual float Eval(int playerID, int enemyID) { DBG(cerr << "EVAL" << endl;); abort(); }
	virtual ~_Game() {}
	//Search Related Stuff
	virtual int minimaxStartingDepth() { DBG(cerr << "miniDepth" << endl;); abort(); }
};


/****************************************  OWARE GAME AND MOVE **************************************************/

class Move_Oware : public _Move {
public:
	int column;
	Move_Oware(int _column) { column = _column; }
	inline int GetType(const Game &S) override { return 0; }
	inline int GetMove(const Game &S) override { return column; }
	string PrintMove(const Game &S) override { return to_string(column); }
	virtual ~Move_Oware() {}
};

class Game_Oware :public _Game {
protected:
	const int ME = 0;
	const int ENEMY = 1;
	static inline void CopyFromOware(Game_Oware& dst, Game_Oware& original) {
		dst.myGameID = original.myGameID;
		dst.turn = original.turn;
		dst.simTurn = original.simTurn;
		dst.idToPlay = original.idToPlay;
		dst.Hash = original.Hash;

		dst.LL[0] = original.LL[0];
		dst.LL[1] = original.LL[1];
		dst.Player2 = original.Player2;
		dst.Score = original.Score;
		dst.inGameSeeds[0] = original.inGameSeeds[0];
		dst.inGameSeeds[1] = original.inGameSeeds[1];
	}
	static vector<Move> ALL_POSSIBLE_MOVES;
	static const int PLAYERS = 2;
	static const int COLUMNS = 6;
	int seedCount[PLAYERS] = { 0 };
	void GenerateAllPossibleMoves() /*override*/ {
		for (int i = 0; i < COLUMNS; ++i)
		{
			Move m = static_pointer_cast<_Move>(make_shared<Move_Oware>(i));
			Game_Oware::ALL_POSSIBLE_MOVES.push_back(m);
		}
	}
public:
	union {
		uint64_t LL[2];
		uint16_t word[8];
		struct {
			uint8_t cells[PLAYERS][COLUMNS];
			uint8_t PlayerTurn;
			uint8_t NextMove;
			uint8_t score[PLAYERS];
		};
	};
	bool Player2 = false;
	float Score = 0.0f;
	uint8_t inGameSeeds[PLAYERS];

	int getTimeLimit() override {
		if (turn == 0) return 800 * 1000; else  return 45 * 1000;
	}
	int getPlayerCount() override { return PLAYERS; }
	int getunitsPerPlayer() override { return 1; }
	void readConfig(Stopwatch& t) override {
		GenerateAllPossibleMoves();
		idToPlay = ME;
		score[ME] = 0;
		score[ENEMY] = 0;
		NextMove = 0;
		PlayerTurn = ME;
	}
	void readTurn(Stopwatch& t) override {
		++turn;
		idToPlay = ME;
		int plantadas = 0;
		for (int P = ME; P < 2; ++P)
		{
			inGameSeeds[P] = 0;
			for (int i = 0; i < 6; ++i)
			{
				int seed;
				cin >> seed; cin.ignore();
				if (i == 0 && P == ME)
				{
					t.Start();
					t.setTimeout(getTimeLimit());
				}
				plantadas += seed;

				if (turn == 0 && seed != 4)
				{
					Player2 = true;
				}

				if (P == ME)
				{
					inGameSeeds[ME] += seed;
					cells[ME][i] = seed;
				}
				else
				{
					inGameSeeds[ENEMY] += seed;
					cells[ENEMY][5 - i] = seed;
				}

			}
		}
		if (turn == 0)
		{
			cerr << "I'm Player" << (Player2 ? "2" : "1") << endl;
		}
		else {
			//Recover enemy score from the game state
			int Calculated = 48 - plantadas - score[ME];
			if (Calculated != score[ENEMY])
			{
				if (Calculated >= 0)  score[ENEMY] = Calculated;
			}
		}
		CalcHash(-1);
		Score = Eval(ME, ENEMY);
#ifdef DBG_MODE			
		for (int i = 0; i < 6; ++i) {
			cerr << (int)cells[Player2 ? ME : ENEMY][i] << ",";
		}
		cerr << endl;
		for (int i = 0; i < 6; ++i) {
			cerr << (int)cells[Player2 ? ENEMY : ME][i] << ",";
		}
		cerr << endl;
#endif		
		cerr << "Points:" << (int)score[ME] << " " << (int)score[ENEMY] << " inGameSeeds:[" << (int)inGameSeeds[ME] << "," << (int)inGameSeeds[ENEMY] << "] Hash:" << Hash << endl;
	}

	inline Move decodeMove(const int& idx) override {
		DBG(assert(idx >= 0 && idx < COLUMNS););
		if (idx >= 0 && idx < COLUMNS)
			return  Game_Oware::ALL_POSSIBLE_MOVES[idx];

		//Failover. That's baaaaaaaad :(
		return  Game_Oware::ALL_POSSIBLE_MOVES[0];

	}

	inline bool isValidMove(const int& id, const int& column)
	{
		bool  enemy_play = (id != ME);

		if (id != ME) {
			if (cells[ENEMY][column] != 0) //cell has seeds, valid move
			{
				if ((cells[ENEMY][column] + column >= 6)
					|| (inGameSeeds[ME] > 0)) return true;
			}
		}
		else {
			if (cells[ME][column] != 0) //cell has seeds, valid move
			{
				if ((cells[ME][column] + column >= 6)
					|| (inGameSeeds[ENEMY] > 0)) return true;
			}
		}

		return false;
	}

	void valid_moves(const int& id, vector<int> &Possible_Moves) override
	{
		Possible_Moves.clear();
		//To do, move ordering: Captures, anti-captures, others
		for (int i = 0; i < COLUMNS; ++i) {
			if (isValidMove(id, i))
				Possible_Moves.push_back(i);
		}
	}


	inline bool isEndGame() override {

		if (turn + simTurn >= 200)
			return true;
		//Winning conditions

		if (score[ENEMY] > 24) {
			return true;
		}
		else if (score[ME] > 24) {
			return true;
		}

		if (idToPlay == ME && inGameSeeds[ME] == 0) return true;
		else if (idToPlay != ME && inGameSeeds[ENEMY] == 0) return true;
		return false;
	}
	void Simulate(Move move) override {
		//I hate the game rules. A lot of yes but no.
		//Grand Slam and forfeits are ugly.
		++SIMCOUNT;
		int start = (static_pointer_cast<Move_Oware>(move))->column;
		++simTurn;
		bool enemy_play = (idToPlay != ME);
		int current_cell;
		int seeds_to_place;
		
		if (enemy_play) {
			current_cell = COLUMNS - 1 - start;
			seeds_to_place = cells[ENEMY][start];
			cells[ENEMY][start] = 0;
		}
		else {

			current_cell = start + COLUMNS;
			seeds_to_place = cells[ME][start];
			cells[ME][start] = 0;
		}

		int start_cell = current_cell;
		while ((seeds_to_place--) != 0) {

			current_cell = (current_cell + 1) % 12;
			if (current_cell == start_cell) {
				seeds_to_place++;
				continue;
			}

			if (current_cell >= 0 && current_cell <= 5) {
				int pos = COLUMNS - 1 - current_cell;
				cells[ENEMY][pos] = cells[ENEMY][pos] + 1;
			}
			else {
				int pos = current_cell - COLUMNS;
				cells[ME][pos] = cells[ME][pos] + 1;
			}
		}

		inGameSeeds[ENEMY] = 0;
		inGameSeeds[ME] = 0;
		for (int i = 0; i < 6; ++i)
		{
			inGameSeeds[ENEMY] += cells[ENEMY][i];
			inGameSeeds[ME] += cells[ME][i];
		}
		
		//Forfeit checking
		if (!enemy_play && (current_cell >= 0 && current_cell <= 5)) {
			int FUTURECAPTURE = 0;
			for (int i = current_cell; i >= 0; --i)
			{
				int pos = COLUMNS - 1 - i;
				int seeds = cells[ENEMY][pos];
				if (seeds == 2 || seeds == 3) {
					FUTURECAPTURE += seeds;
					continue;
				}
				else {
					break;
				}
			}

			if (FUTURECAPTURE != inGameSeeds[ENEMY]) //Forfeit all seeds
				for (int i = current_cell; i >= 0; --i)
				{
					int pos = COLUMNS - 1 - i;
					int seeds = cells[ENEMY][pos];
					if (seeds == 2 || seeds == 3) {
						score[ME] += seeds;
						cells[ENEMY][pos] = 0;
					}
					else {
						break;
					}
				}
		}
		//Seed capture from player cells [enemy turn]
		else if (enemy_play && (current_cell >= 6 && current_cell <= 11)) {
			int FUTURECAPTURE = 0;
			for (int i = current_cell; i >= 6; --i)
			{
				int pos = i - COLUMNS;
				int seeds = cells[ME][pos];
				if (seeds == 2 || seeds == 3) {
					FUTURECAPTURE += seeds;
				}
				else {
					break;
				}
			}

			if (FUTURECAPTURE != inGameSeeds[ME]) //Forfeit all seeds	
				for (int i = current_cell; i >= 6; --i)
				{
					int pos = i - COLUMNS;
					int seeds = cells[ME][pos];
					if (seeds == 2 || seeds == 3) {
						score[ENEMY] += seeds;
						cells[ME][pos] = 0;
						continue;
					}
					else {
						break;
					}
				}
		}
		idToPlay = 1 - idToPlay;
		CalcHash(-1);
	}

	
	uint64_t CalcHash(const int& _NextMove)noexcept override {
		NextMove = _NextMove;
		const uint64_t MASK1 = 0x0101010101010101UL; //8 bits
		const uint64_t MASK2 = 0x001F1F0101010101UL; //4 bits seed + 1 player + 3 move + 10 score = 18

		const uint64_t MASK1B = 0x0E0E0E0E0E0E0E0EUL; //remaining bits 3*8 = 24 
		const uint64_t MASK2B = 0x000000000E0E0E0EUL; //remaining bits 3*4 = 12

		if (NextMove >= 0)
		{
			Hash = (uint64_t)(NextMove & 0x07)
				+ (_pext_u64(LL[0], MASK1) << (3))
				+ (_pext_u64(LL[1], MASK2) << (3 + 8))
				+ (_pext_u64(LL[0], MASK1B) << (3 + 8 + 18))
				+ (_pext_u64(LL[1], MASK2B) << (3 + 8 + 18 + 24));
		}
		else {
			Hash =
				(_pext_u64(LL[0], MASK1))
				+ (_pext_u64(LL[1], MASK2) << (8))
				+ (_pext_u64(LL[0], MASK1B) << (8 + 18))
				+ (_pext_u64(LL[1], MASK2B) << (8 + 18 + 24));
		}
		return Hash;
	}


	int getPackSize()noexcept override { return 16;/*bytes*/ }
	void Pack(uint8_t* g)noexcept override {}
	void Unpack(uint8_t* g)noexcept override {}
	void CopyFrom(Game original) override { CopyFromOware(*this, *static_pointer_cast<Game_Oware>(original)); }
	void UnSimulate(Game prev, Move move) override { CopyFromOware(*this, *static_pointer_cast<Game_Oware>(prev)); }
	Game_Oware() {
		//GenerateAllPossibleMoves();
	}
	Game_Oware(shared_ptr<Game_Oware> original) {
		CopyFromOware(*this, *original);
	}
	Game Clone() override {
		Game g = std::make_shared<Game_Oware>();
		g->CopyFrom(this->getptr());
		return g;
	}
	virtual ~Game_Oware() {
	}
	inline int minimaxStartingDepth() override { return 7; }
	float Eval(int playerID, int enemyID)override; //Implemented below
};
vector<Move> Game_Oware::ALL_POSSIBLE_MOVES;



/****************************************  NEGAMAX CODE **************************************************/

float negamax(Game& game, int8_t unitId, uint8_t depth, float alpha, float beta) {
	if (stopwatch.Timeout()) return 0;

	float score = -FLT_MAX;
	if (depth == 0 || game->isEndGame()) {
		score = game->Eval(unitId, 1 - unitId);
		return score;
	}

	vector<int> moveList;
	game->valid_moves(unitId, moveList);

	if (moveList.size() == 0) { //This is also an endgame
		score = game->Eval(unitId, 1 - unitId);
		return score;
	}

	for (uint8_t m = 0; m < moveList.size(); ++m) {
		Game local = game->Clone();
		local->idToPlay = unitId;
		local->Simulate(local->decodeMove(moveList[m]));

		score = -negamax(local, (1 - unitId), depth - 1, -beta, -alpha);

		if (stopwatch.Timeout()) 
			return 0;

		if (score > alpha) {
			if (score >= beta) {
				alpha = beta;
				break;
			}
			alpha = score;
		}
	}

	return alpha;
}

inline Move negamax(const Game gamestate, Move& getBestMove) {
	const int MAX_DEPTH = 20;
	int bestMove = -1;
	SIMCOUNT = 0;
	float bestScore = 0.0f;
	int depth = -1;
	//I try to do a simple depth=0 move ordering, based on previous iterations
	vector<pair<int,float>> orderedMoves;

	//If I have free time I try to search deeper
	for (depth = gamestate->minimaxStartingDepth(); depth < MAX_DEPTH; depth+=2) {
		float alpha = -FLT_MAX;
		float beta = FLT_MAX;
		int currBestMove;
		//On the first loop I populate orderedMoves
		if (orderedMoves.size() ==0)
		{
			vector<int> moveList;
			gamestate->valid_moves(MY_ID, moveList);
			for (auto&move:moveList)
			orderedMoves.push_back( pair<int,float>{move,0.0f});
		}
		else {
			//Sort based on previous iteration scores
			sort(orderedMoves.begin(),orderedMoves.end(),[](const auto& a, const auto& b){return a.second > b.second;});
		}

		//Search Root loop.
		for (int m = 0; m < orderedMoves.size(); ++m) {
			//Create a copy of the Game State
			Game local = gamestate->Clone();
			local->idToPlay = MY_ID;
			//Do the move
			local->Simulate(local->decodeMove(orderedMoves[m].first));

			//Do a negamax
			float score = -negamax(local, (1 - MY_ID), depth - 1, -beta, -alpha);
			//Update the score on the ordered Moves
			orderedMoves[m].second = score;
			//If there is no time just exit
			if (stopwatch.Timeout())
				break;
			//It's a better score than previous, update bestMove and alpha
			if (score > alpha) {
				currBestMove = orderedMoves[m].first;
				if (score >= beta) {
					alpha = beta;
					break;
				}
				alpha = score;
			}
		}
		//No more time
		if (stopwatch.Timeout())
			break;
		//Only update when there isn't a timeout
		bestScore = alpha;
		bestMove = currBestMove;
	}
	if (bestMove < 0)
		cerr << "Error, not simulated!!!, decrease the value of minimaxStartingDepth()" << endl;
	cerr << "Depth:" << (int)depth << " T:" << stopwatch.EllapsedMilliseconds() << "ms Score:" << bestScore << "\n";
	return gamestate->decodeMove(bestMove);
}


/****************************************  EVALUATION FUNCTION *******************************************/
float Game_Oware::Eval(int playerID, int enemyID) {
	const float K_SCORE = 30.0f;
	const float K_SEEDS = 2.0f;
	
	const float K_KROO = 50.0f;
	const float K_EMPTY = 3.0f;
	const float K_CAPTURABLE = 15.0f;

	const float H3 = 0.0f;

	//Endgame scoring
	if (score[ENEMY] > 24)
	{
		Score = -(130 - simTurn)*K_SCORE;
		return Score;
	}
	else if (score[ME] > 24) {
			Score = (130 - simTurn)*K_SCORE;
			return Score;
		}

	if ((idToPlay == ME && inGameSeeds[ME] == 0)
		|| (idToPlay != ME && inGameSeeds[ENEMY] == 0)) {

		if (score[ME] + inGameSeeds[ME] > score[ENEMY] + inGameSeeds[ENEMY])
		{
			Score = (120 - simTurn)*K_SCORE;
		}
		else if (score[ME] + inGameSeeds[ME] < score[ENEMY] + inGameSeeds[ENEMY])
		{
			Score = -(120 - simTurn)*K_SCORE;
		}
		else Score = simTurn; //Draw
		return Score;
	}

	if (turn + simTurn >= 200)
	{
		if (score[ME] > score[ENEMY])
		{
			Score = (110 - simTurn)*K_SCORE;
		}
		else if (score[ME] < score[ENEMY])
		{
			Score = -(110 - simTurn)*K_SCORE;
		}
		else Score = simTurn;
		if (ME == playerID)
		  return Score;
		else return -Score;
	}
	
    //Normal Scoring
	int MovesToChoose = 0;
	if (H3 != 0.0f)
	{
		vector<int> PosMoves;
		valid_moves(idToPlay, PosMoves);
		MovesToChoose = -(int)PosMoves.size();
	}


    Score  = (float)(score[ME] - score[ENEMY])*K_SCORE;
    Score += (float)(inGameSeeds[ME] - inGameSeeds[ENEMY])*K_SEEDS;
    Score += (float)MovesToChoose*H3;
    
    int capturable[2] = {0};
    int empty[2] = {0};
    int kroo[2] = {0};
    for (int p=0;p<2;++p)
	for (int i = 0; i < 6; ++i)
	{
		if (cells[p][i] == 0)  ++empty[p];
		if (cells[p][i] < 3)  ++capturable[p];
		if (cells[p][i] > 12)  ++kroo[p];
	}
    Score -= (float)capturable[ME]*capturable[ME]*K_CAPTURABLE;
    Score -= (float)empty[ME]*empty[ME]*K_EMPTY;
    Score += (float)kroo[ME]*K_KROO;
    Score += (float)capturable[ME]*capturable[ME]*K_CAPTURABLE;
    Score += (float)empty[ME]*empty[ME]*K_EMPTY;
    Score -= (float)kroo[ME]*K_KROO;

	
	if (playerID != ME)
	return -Score;
	else return Score;
}


int main() {
	Game gamestate = std::make_shared<Game_Oware>();
	gamestate->readConfig(stopwatch);
	while (true)
	{
		gamestate->readTurn(stopwatch);
		Move bestMove = negamax(gamestate, bestMove);
		cout << bestMove->PrintMove(gamestate) << endl;
		//I need to apply my move to calculate my score.
		//Based on my own score I can infer the enemy score at readTurn
		gamestate->Simulate(bestMove);
	}
}

/*

http://www.joansala.com/auale/strategy/en/
For each seed the player has captured add 2 points. Add 3 points for each 
hole in the opponent's row containing one or two seeds; add 4 points for
each of the opponent's holes containing no seeds and 2 extra points 
if the player has accumulated over twelve seeds in any of his own holes.
After doing the same calculation from the viewpoint of the opponent,
it will be easy to know which of the two players has an obvious advantage.

https://www.politesi.polimi.it/bitstream/10589/134455/3/Thesis.pdf
H1: Hoard as many counters as possible in one pit. This heuristic,with a look ahead of one move, works by attempting to keep as many
counters as possible in the left-most pit on the board. At the end of
the game, all the counters on a side of the board are awarded to that
player’s side. There is some evidence in literature that this is the best
pit in which hoard counters [15].
• H2: Keep as many counters on the players own side. This heuristic is
a generalized version of H1 and is based on the same principles.
• H3: Have as many moves as possible from which to choose. This
heuristic has a look ahead of one and explores the possible benefit of
having more moves to choose from.
• H4: Maximize the amount of counters in a players own store. This
heuristic aims to pick a move that will maximize the amount of counters captured. It has a look ahead of one.
• H5: Move the counters from the pit closest to the opponents side.
This heuristic, with a look ahead of one, aims to make a move from
the right-most pit on the board. If it is empty, then the next pit is
checked and so on. It was chosen because it has a good performance
in Kalah [18] and the perfect player’s opening move in Awari is to play
from the rightmost pit [24].
• H6: Keep the opponents score to a minimum. This heuristic, with a
look ahead of two moves, attempts to minimize the number of counters
an opponent can win on their next move.

Heuristic H1 H2 H3 H4 H5 H6
Weight 0.198649 0.190084 0.370793 1 0.418841 0.565937
V = H1 × W1 + H2 × W2 + H3 × W3 + H4 × W4 + H5 × W5 − H6 × W6


*/
