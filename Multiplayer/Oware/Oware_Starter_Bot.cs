using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
/*
Starter bot for Oware Abapa https://www.codingame.com/ide/puzzle/oware-abapa
Implements basic simulation + Negamax Search + Scoring
It works right out of the box. You can change eval in line 470+
it's the function float Game_Oware.Eval(int playerID, int enemyID)

You can also change the search algorithm for another thing (minimax, MCTS).

Simulation is pretty basic, not optimized.
*/

namespace Oware
{
	class Player
	{
		public static int MY_ID = 0;
		public static Stopwatch stopwatch = new Stopwatch();
		public static int SIMCOUNT = 0;
		const float FLT_MAX = Single.MaxValue;
		abstract class Move
		{
			public abstract int GetType(Game S);
			public abstract int GetMove(Game S);
			public abstract string PrintMove(Game S);
			public Move getptr() { return this; }
			~Move() { }
		};

		abstract class Game
		{
			public int myGameID = 0; //Holds real ID from the game
			public int turn = -1;  //turn from the game
			public int simTurn = 0; //Simulated turn == depth
			public int idToPlay;  //Current playing ID
			public UInt64 Hash;
			public Game getptr() { return this; }
			public virtual void swap_turn() { idToPlay = 1 - idToPlay; }
			public virtual int getTimeLimit() { if (turn == 0) return 800 /* * 1000*/; else return 40 /* * 1000*/; }
			//abstract void GenerateAllPossibleMoves() { DBG(cerr<<"GENALL"<<endl;);abort();}
			public abstract int getPlayerCount();
			public abstract int getunitsPerPlayer();
			public abstract void readConfig(Stopwatch t);
			public abstract void readTurn(Stopwatch t);
			public abstract Move decodeMove(int idx);
			public abstract void valid_moves(int id, List<int> Possible_Moves);

			public abstract UInt64 CalcHash(int _NextMove);
			public abstract int getPackSize();
			/*public abstract void Pack(int* g);
			public abstract void Unpack(int* g);*/

			public abstract bool isEndGame();
			public abstract void Simulate(Move move);
			public abstract void CopyFrom(Game original);
			public abstract void UnSimulate(Game g, Move move);
			public abstract Game Clone();
			public abstract float Eval(int playerID, int enemyID);
			~Game() { }
			//Search Related Stuff
			public abstract int minimaxStartingDepth();
		};

		/****************************************  OWARE GAME AND MOVE **************************************************/

		class Move_Oware : Move
		{
			public int column;
			public Move_Oware(int _column) { column = _column; }
			public override int GetType(Game S) { return 0; }
			public override int GetMove(Game S) { return column; }
			public override string PrintMove(Game S) { return column.ToString(); }
			~Move_Oware() { }
		};


		class Game_Oware : Game
		{
			public static List<Move> ALL_POSSIBLE_MOVES = new List<Move>();
			public const int ME = 0;
			public const int ENEMY = 1;
			public const int PLAYERS = 2;
			public const int COLUMNS = 6;

			public int[,] cells = new int[PLAYERS, COLUMNS];
			public int PlayerTurn;
			public int NextMove;
			public bool Player2 = false;
			public float Score = 0.0f;

			public int[] score = new int[PLAYERS];
			public int[] inGameSeeds = new int[PLAYERS];
			public int[] seedCount = new int[PLAYERS];

			public static void CopyFromOware(Game_Oware dst, Game_Oware original)
			{ //Deep Copy
			  //From base Game
				dst.myGameID = original.myGameID;
				dst.turn = original.turn;
				dst.simTurn = original.simTurn;
				dst.idToPlay = original.idToPlay;
				dst.Hash = original.Hash;
				//Specific to Oware
				dst.PlayerTurn = original.PlayerTurn;
				dst.NextMove = original.NextMove;
				dst.Player2 = original.Player2;
				dst.Score = original.Score;

				for (int p = 0; p < PLAYERS; ++p)
				{
					dst.score[p] = original.score[p];
					dst.inGameSeeds[p] = original.inGameSeeds[p];
					dst.seedCount[p] = original.seedCount[p];
					for (int i = 0; i < COLUMNS; ++i)
					{
						dst.cells[p, i] = original.cells[p, i];
					}
				}
			}

			void GenerateAllPossibleMoves()
			{
				for (int i = 0; i < COLUMNS; ++i)
				{
					Move m = new Move_Oware(i);
					ALL_POSSIBLE_MOVES.Add(m);
				}
			}


			public override int getTimeLimit()
			{
				if (turn == 0) return 800 /* * 1000*/; else return 40 /* * 1000*/;
			}
			public override int getPlayerCount() { return PLAYERS; }
			public override int getunitsPerPlayer() { return 1; }
			public override void readConfig(Stopwatch t)
			{
				GenerateAllPossibleMoves();
				idToPlay = ME;
				score[ME] = 0;
				score[ENEMY] = 0;
				NextMove = 0;
				PlayerTurn = ME;
			}
			public override void readTurn(Stopwatch t)
			{
				++turn;
				idToPlay = ME;
				int plantadas = 0;
				string[] inputs = Console.ReadLine().Split(' ');
				for (int P = ME; P < 2; ++P)
				{
					inGameSeeds[P] = 0;
					for (int i = 0; i < 6; ++i)
					{
						int seed = int.Parse(inputs[i + P * 6]);
						if (i == 0 && P == ME)
						{
							t.Start();
							t.Restart();
						}
						plantadas += seed;

						if (turn == 0 && seed != 4)
						{
							Player2 = true;
						}

						if (P == ME)
						{
							inGameSeeds[ME] += seed;
							cells[ME, i] = seed;
						}
						else
						{
							inGameSeeds[ENEMY] += seed;
							cells[ENEMY, 5 - i] = seed;
						}

					}
				}
				if (turn == 0)
				{
					Console.Error.WriteLine("I'm Player" + (Player2 ? "2" : "1"));
				}
				else
				{
					//Recover enemy score from the game state
					int Calculated = 48 - plantadas - score[ME];
					if (Calculated != score[ENEMY])
					{
						if (Calculated >= 0) score[ENEMY] = Calculated;
					}
				}
				CalcHash(-1);
				Score = Eval(ME, ENEMY);
				//#define DBG_MODE
#if DBG_MODE
				for (int i = 0; i < 6; ++i)
				{
					Console.Error.Write((int)cells[Player2 ? ME : ENEMY, i] + ",");
				}
				Console.Error.WriteLine();
				for (int i = 0; i < 6; ++i)
				{
					Console.Error.Write((int)cells[Player2 ? ENEMY : ME, i] + ",");
				}
				Console.Error.WriteLine();
#endif
				Console.Error.WriteLine((int)score[ME] + " " + (int)score[ENEMY] + " inGameSeeds:[" + (int)inGameSeeds[ME] + "," + (int)inGameSeeds[ENEMY] + "] Hash:" + Hash);
			}

			public override Move decodeMove(int idx)
			{
				if (idx >= 0 && idx < COLUMNS)
					return ALL_POSSIBLE_MOVES[idx];

				//Failover. That's baaaaaaaad :(
				return ALL_POSSIBLE_MOVES[0];

			}

			bool isValidMove(int id, int column)
			{
				bool enemy_play = (id != ME);

				if (id != ME)
				{
					if (cells[ENEMY, column] != 0) //cell has seeds, valid move
					{
						if ((cells[ENEMY, column] + column >= 6)
							|| (inGameSeeds[ME] > 0)) return true;
					}
				}
				else
				{
					if (cells[ME, column] != 0) //cell has seeds, valid move
					{
						if ((cells[ME, column] + column >= 6)
							|| (inGameSeeds[ENEMY] > 0)) return true;
					}
				}

				return false;
			}

			public override void valid_moves(int id, List<int> Possible_Moves)
			{
				Possible_Moves.Clear();
				//To do, move ordering: Captures, anti-captures, others
				for (int i = 0; i < COLUMNS; ++i)
				{
					if (isValidMove(id, i))
						Possible_Moves.Add(i);
				}
			}


			public override bool isEndGame()
			{

				if (turn + simTurn >= 200)
					return true;
				//Winning conditions

				if (score[ENEMY] > 24)
				{
					return true;
				}
				else if (score[ME] > 24)
				{
					return true;
				}

				if (idToPlay == ME && inGameSeeds[ME] == 0) return true;
				else if (idToPlay != ME && inGameSeeds[ENEMY] == 0) return true;
				return false;
			}
			public override void Simulate(Move move)
			{
				//I hate the game rules. A lot of yes but no.
				//Grand Slam and forfeits are ugly.
				++SIMCOUNT;
				int start = ((Move_Oware)move).column;
				++simTurn;
				bool enemy_play = (idToPlay != ME);
				int current_cell;
				int seeds_to_place;

				if (enemy_play)
				{
					current_cell = COLUMNS - 1 - start;
					seeds_to_place = cells[ENEMY, start];
					cells[ENEMY, start] = 0;
				}
				else
				{

					current_cell = start + COLUMNS;
					seeds_to_place = cells[ME, start];
					cells[ME, start] = 0;
				}

				int start_cell = current_cell;
				while ((seeds_to_place--) != 0)
				{

					current_cell = (current_cell + 1) % 12;
					if (current_cell == start_cell)
					{
						seeds_to_place++;
						continue;
					}

					if (current_cell >= 0 && current_cell <= 5)
					{
						int pos = COLUMNS - 1 - current_cell;
						cells[ENEMY, pos] = cells[ENEMY, pos] + 1;
					}
					else
					{
						int pos = current_cell - COLUMNS;
						cells[ME, pos] = cells[ME, pos] + 1;
					}
				}

				inGameSeeds[ENEMY] = 0;
				inGameSeeds[ME] = 0;
				for (int i = 0; i < 6; ++i)
				{
					inGameSeeds[ENEMY] += cells[ENEMY, i];
					inGameSeeds[ME] += cells[ME, i];
				}

				//Forfeit checking
				if (!enemy_play && (current_cell >= 0 && current_cell <= 5))
				{
					int FUTURECAPTURE = 0;
					for (int i = current_cell; i >= 0; --i)
					{
						int pos = COLUMNS - 1 - i;
						int seeds = cells[ENEMY, pos];
						if (seeds == 2 || seeds == 3)
						{
							FUTURECAPTURE += seeds;
							continue;
						}
						else
						{
							break;
						}
					}

					if (FUTURECAPTURE != inGameSeeds[ENEMY]) //Forfeit all seeds
						for (int i = current_cell; i >= 0; --i)
						{
							int pos = COLUMNS - 1 - i;
							int seeds = cells[ENEMY, pos];
							if (seeds == 2 || seeds == 3)
							{
								score[ME] += seeds;
								cells[ENEMY, pos] = 0;
							}
							else
							{
								break;
							}
						}
				}
				//Seed capture from player cells [enemy turn]
				else if (enemy_play && (current_cell >= 6 && current_cell <= 11))
				{
					int FUTURECAPTURE = 0;
					for (int i = current_cell; i >= 6; --i)
					{
						int pos = i - COLUMNS;
						int seeds = cells[ME, pos];
						if (seeds == 2 || seeds == 3)
						{
							FUTURECAPTURE += seeds;
						}
						else
						{
							break;
						}
					}

					if (FUTURECAPTURE != inGameSeeds[ME]) //Forfeit all seeds	
						for (int i = current_cell; i >= 6; --i)
						{
							int pos = i - COLUMNS;
							int seeds = cells[ME, pos];
							if (seeds == 2 || seeds == 3)
							{
								score[ENEMY] += seeds;
								cells[ME, pos] = 0;
								continue;
							}
							else
							{
								break;
							}
						}
				}
				idToPlay = 1 - idToPlay;
				CalcHash(-1);
			}


			public override UInt64 CalcHash(int _NextMove)
			{
				NextMove = _NextMove;
				const UInt64 MASK1 = 0x0101010101010101UL; //8 bits
				const UInt64 MASK2 = 0x001F1F0101010101UL; //4 bits seed + 1 player + 3 move + 10 score = 18

				const UInt64 MASK1B = 0x0E0E0E0E0E0E0E0EUL; //remaining bits 3*8 = 24 
				const UInt64 MASK2B = 0x000000000E0E0E0EUL; //remaining bits 3*4 = 12

				/* //Too lazy to code that in C#/Java 
								ParallelBitExtract // _pext_u64

						if (NextMove >= 0)
						{
							Hash = (UInt64) (NextMove & 0x07)
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
						*/
				return Hash;
			}


			public override int getPackSize() { return 16;/*ints*/ }
			/*public override void Pack(int* g)noexcept override {}
			public override void Unpack(int* g)noexcept override {}*/
			public override void CopyFrom(Game original) { CopyFromOware(this, (Game_Oware)original); }
			public override void UnSimulate(Game prev, Move move) { CopyFromOware(this, (Game_Oware)prev); }
			public Game_Oware()
			{
				//GenerateAllPossibleMoves();
			}
			Game_Oware(Game_Oware original)
			{
				CopyFromOware(this, original);
			}
			public override Game Clone()
			{
				Game g = new Game_Oware();
				g.CopyFrom(this.getptr());
				return g;
			}
			~Game_Oware()
			{
			}
			public override int minimaxStartingDepth() { return 5; }
			/****************************************  EVALUATION FUNCTION *******************************************/
			public override float Eval(int playerID, int enemyID)
			{
				const float K_SCORE = 30.0f;
				const float K_SEEDS = 2.0f;

				const float K_KROO = 50.0f;
				const float K_EMPTY = 3.0f;
				const float K_CAPTURABLE = 15.0f;

				float H3 = 0.0f;

				//Endgame scoring
				if (score[ENEMY] > 24)
				{
					Score = -(130 - simTurn) * K_SCORE;
					return Score;
				}
				else if (score[ME] > 24)
				{
					Score = (130 - simTurn) * K_SCORE;
					return Score;
				}

				if ((idToPlay == ME && inGameSeeds[ME] == 0)
					|| (idToPlay != ME && inGameSeeds[ENEMY] == 0))
				{

					if (score[ME] + inGameSeeds[ME] > score[ENEMY] + inGameSeeds[ENEMY])
					{
						Score = (120 - simTurn) * K_SCORE;
					}
					else if (score[ME] + inGameSeeds[ME] < score[ENEMY] + inGameSeeds[ENEMY])
					{
						Score = -(120 - simTurn) * K_SCORE;
					}
					else Score = simTurn; //Draw
					return Score;
				}

				if (turn + simTurn >= 200)
				{
					if (score[ME] > score[ENEMY])
					{
						Score = (110 - simTurn) * K_SCORE;
					}
					else if (score[ME] < score[ENEMY])
					{
						Score = -(110 - simTurn) * K_SCORE;
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
					List<int> PosMoves = new List<int>();
					valid_moves(idToPlay, PosMoves);
					MovesToChoose = -(int)PosMoves.Count;
				}


				Score = (float)(score[ME] - score[ENEMY]) * K_SCORE;
				Score += (float)(inGameSeeds[ME] - inGameSeeds[ENEMY]) * K_SEEDS;
				Score += (float)MovesToChoose * H3;

				int[] capturable = new int[PLAYERS];
				int[] empty = new int[PLAYERS];
				int[] kroo = new int[PLAYERS];
				for (int p = 0; p < 2; ++p)
					for (int i = 0; i < 6; ++i)
					{
						if (cells[p, i] == 0) ++empty[p];
						if (cells[p, i] < 3) ++capturable[p];
						if (cells[p, i] > 12) ++kroo[p];
					}
				Score -= (float)capturable[ME] * capturable[ME] * K_CAPTURABLE;
				Score -= (float)empty[ME] * empty[ME] * K_EMPTY;
				Score += (float)kroo[ME] * K_KROO;
				Score += (float)capturable[ME] * capturable[ME] * K_CAPTURABLE;
				Score += (float)empty[ME] * empty[ME] * K_EMPTY;
				Score -= (float)kroo[ME] * K_KROO;


				if (playerID != ME)
					return -Score;
				else return Score;
			}
		}


		/****************************************  NEGAMAX CODE **************************************************/

		static float negamax(Game game, int unitId, int depth, float alpha, float beta)
		{
			if (stopwatch.ElapsedMilliseconds > game.getTimeLimit()) return 0;

			float score = -FLT_MAX;
			if (depth == 0 || game.isEndGame())
			{
				score = game.Eval(unitId, 1 - unitId);
				return score;
			}

			List<int> moveList = new List<int>();
			game.valid_moves(unitId, moveList);

			if (moveList.Count == 0)
			{ //This is also an endgame
				score = game.Eval(unitId, 1 - unitId);
				return score;
			}

			for (int m = 0; m < moveList.Count; ++m)
			{
				Game local = game.Clone();
				local.idToPlay = unitId;
				local.Simulate(local.decodeMove(moveList[m]));

				score = -negamax(local, (1 - unitId), depth - 1, -beta, -alpha);

				if (stopwatch.ElapsedMilliseconds > game.getTimeLimit())
					return 0;

				if (score > alpha)
				{
					if (score >= beta)
					{
						alpha = beta;
						break;
					}
					alpha = score;
				}
			}

			return alpha;
		}


		class Pair<Key, Val>
		{
			public Key first { get; set; }
			public Val second { get; set; }

			public Pair() { }

			public Pair(Key key, Val val)
			{
				this.first = key;
				this.second = val;
			}
		}

		static Move negamax(Game gamestate)
		{
			const int MAX_DEPTH = 20;
			int bestMove = -1;
			SIMCOUNT = 0;
			float bestScore = 0.0f;
			int depth = -1;

			//I try to do a simple depth=0 move ordering, based on previous iterations
			List<Pair<int, float>> orderedMoves = new List<Pair<int, float>>();

			//If I have free time I try to search deeper
			for (depth = gamestate.minimaxStartingDepth(); depth < MAX_DEPTH; depth += 2)
			{
				float alpha = -FLT_MAX;
				float beta = FLT_MAX;
				int currBestMove = -1;
				//On the first loop I populate orderedMoves
				if (orderedMoves.Count == 0)
				{
					List<int> moveList = new List<int>();
					gamestate.valid_moves(MY_ID, moveList);
					foreach (int move in moveList)
						orderedMoves.Add(new Pair<int, float>(move, 0.0f));
				}
				else
				{
					//Sort based on previous iteration scores
					orderedMoves.OrderByDescending(a => a.second);
				}

				//Search Root loop.
				for (int m = 0; m < orderedMoves.Count; ++m)
				{
					//Create a copy of the Game State
					Game local = gamestate.Clone();
					local.idToPlay = MY_ID;
					//Do the move
					local.Simulate(local.decodeMove(orderedMoves[m].first));

					//Do a negamax
					float score = -negamax(local, (1 - MY_ID), depth - 1, -beta, -alpha);
					//Update the score on the ordered Moves
					orderedMoves[m].second = score;
					//If there is no time just exit
					if (stopwatch.ElapsedMilliseconds > local.getTimeLimit())
						break;
					//It's a better score than previous, update bestMove and alpha
					if (score > alpha)
					{
						currBestMove = orderedMoves[m].first;
						if (score >= beta)
						{
							alpha = beta;
							break;
						}
						alpha = score;
					}
				}
				//No more time
				if (stopwatch.ElapsedMilliseconds > gamestate.getTimeLimit())
					break;
				//Only update when there isn't a timeout
				bestScore = alpha;
				bestMove = currBestMove;
			}
			if (bestMove < 0)
				Console.Error.WriteLine("Error, not simulated!!!, decrease the value of minimaxStartingDepth()");

			Console.Error.WriteLine("Depth:" + (int)depth + " T:" + stopwatch.ElapsedMilliseconds + "ms Sims:"+SIMCOUNT+" Score:" + bestScore);

			return gamestate.decodeMove(bestMove);
		}


		static void Main(string[] args)
		{
			Game gamestate = new Game_Oware();
			gamestate.readConfig(stopwatch);
			while (true)
			{
				gamestate.readTurn(stopwatch);
				Move bestMove = negamax(gamestate);
				Console.WriteLine(bestMove.PrintMove(gamestate));
				//I need to apply my move to calculate my score.
				//Based on my own score I can infer the enemy score at readTurn
				gamestate.Simulate(bestMove);
			}
		}
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
