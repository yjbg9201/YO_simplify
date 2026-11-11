#include "../config.h"

#if defined(ENABLE_TEST_CMD)

// ----------------------------------
//      詰み関係のtestコマンド
// ----------------------------------

// "test genmate ..."のように"test"コマンドの後続コマンドとして書く。

#include <sstream>

#include "../mate/mate.h"

#include "../position.h"
#include "../usi.h"
#include "../thread.h"
#include "../search.h"

using namespace std;
namespace YaneuraOu {
namespace {

	// ----------------------------------
	//      "test matebench" command
	// ----------------------------------

	// 詰みルーチンに関するbenchをとる。
	// このbenchには、"test genmate"コマンドで生成した詰み局面を利用すると良い。
	void mate_bench(IEngine& engine, std::istringstream& is)
	{
		auto& options = engine.get_options();
		auto& threads = engine.get_threads();

		Position pos;

#if !(defined (USE_MATE_SOLVER) || defined(USE_MATE_DFPN))
		cout << "Error! define USE_MATE_SOLVER or USE_MATE_DFPN" << endl;
#else

		// 読み込む問題数
		size_t num = 10000;

		// 問題を何回ずつ解かせるのか。
		// sfen文字列からPositionにセットする時間がロスするので、何度かずつ同じを局面を繰り返し解かせる時に用いる。
		size_t loop = 1;

		// 詰み局面のファイル名(genmateコマンドで生成したものを指定すると良い)
		string filename = "mate.sfen";

		// mate_odd_plyで読む手数
		int mate_ply = 3;

		// ノード数制限
		u32 nodes_limit = 10000;

		// dfpn用のメモリ[MB]
		u32 dfpn_mem = 1024;

		// dfpn-hash table用のメモリ[MB]
		u32 dfpn_hash = 1024;

		// デバッグ用に、解けなかった問題を出力する。
		bool verbose = false;

		int test_mode = 3; // mate_odd_plyとdfpnと両方のテストを行う。

		std::string token;
		while (is >> token)
		{
			if (token == "num")
				is >> num;
			else if (token == "loop")
				is >> loop;
			else if (token == "file")
				is >> filename;
			else if (token == "mate_ply")
				is >> mate_ply;
			else if (token == "nodes")
				is >> nodes_limit;
			else if (token == "verbose")
				verbose = true;
			else if (token == "mode")
				is >> test_mode;

#if defined(USE_MATE_DFPN)
			else if (token == "dfpn_mem")
				is >> dfpn_mem;
			else if (token == "dfpn_hash")
				is >> dfpn_hash;
#endif
		}

		std::cout << "mate bench :" << std::endl
			<< " num (number of problems) = " << num << endl
			<< " sfen file name           = " << filename << endl
			<< " mate_ply                 = " << mate_ply << endl
			<< " nodes(nodes limit)       = " << nodes_limit << endl
			<< " verbose                  = " << verbose << endl
			<< " mode                     = " << test_mode << endl
#if defined(USE_MATE_DFPN)
			<< " dfpn_mem [MB]            = " << dfpn_mem << endl
			<< " dfpn_hash[MB]            = " << dfpn_hash << endl
#endif
			;

#if defined(USE_MATE_DFPN)
		Mate::MateHashTable mate_hash;
		mate_hash.resize(dfpn_hash);

		Mate::Dfpn::MateDfpnSolver dfpn1(Mate::Dfpn::DfpnSolverType::Node32bit);
		dfpn1.alloc(dfpn_mem);

		Mate::Dfpn::MateDfpnSolver dfpn2(Mate::Dfpn::DfpnSolverType::Node16bitOrdering);
		dfpn2.alloc(dfpn_mem);

		Mate::Dfpn::MateDfpnSolver dfpn3(Mate::Dfpn::DfpnSolverType::Node48bitOrdering);
		dfpn3.alloc(dfpn_mem);

		Mate::Dfpn::MateDfpnSolver dfpn4(Mate::Dfpn::DfpnSolverType::Node32bitWithHash);
		dfpn4.alloc(dfpn_mem);
		dfpn4.set_hash_table(&mate_hash);

		Mate::Dfpn::MateDfpnSolver dfpn5(Mate::Dfpn::DfpnSolverType::Node48bitOrderingWithHash);
		dfpn5.alloc(dfpn_mem);
		dfpn5.set_hash_table(&mate_hash);

#endif

		ifstream f(filename);
		unique_ptr<vector<string>> problems = make_unique<vector<string>>();
		problems->reserve(num);

		cout << "read problems.." << endl;
		for (size_t i = 0; i < num; ++i)
		{
			string sfen;
			if (!getline(f, sfen) || f.eof())
				break;
			problems->emplace_back(sfen);

			// 読み込み経過を出力
			if (i % 10000 == 0 && i > 0)
				cout << ".";
		}
		auto problem_num = problems->size();
		cout << "..number of problems read = " << problem_num << endl;

		if (problem_num == 0)
			return;

		string sfen2;
		u64 nodes_searched;

		auto bench = [&](function<Move()> solver, string test_name) {
			ElapsedTimer timer;
			timer.reset();

			cout << "===== " << test_name << " =====" << endl
				<< "test start" << endl;

			// 解けた問題数
			u32 solved = 0;
			nodes_searched = 0;

			TimePoint last_pv = 0;

			TimePoint elapsed = 0;

			// 詰み探索を呼び出した回数。
			size_t times = 0;
			
			auto output_pv = [&]()
			{
				// mateを呼び出した回数。
				cout << " number of times = " << times << " , elapsed = " << elapsed
					<< " , called per second = " << (double)times * 1000 / (elapsed + 0.00000001)
					<< " , solved per second = " << (double)solved * 1000 / (elapsed + 0.00000001)
					<< " , nps = " << nodes_searched * 1000 / elapsed
					<< " , solved = " << solved << " , accuracy = " << (100.0 * solved / times) << "%" << endl;
			};

			size_t size = min(num, problem_num);
			for (size_t i = 0; i < size; ++i)
			{
				elapsed = timer.elapsed();
				// pvは一定間隔で出力する。
				if (last_pv + 2000 < elapsed)
				{
					last_pv = elapsed;
					times = i * loop;
					
					output_pv();
				}

				auto sfen = (*problems)[i];
				sfen2 = sfen;
				StateInfo si;
				pos.set(sfen, &si);
				Move move;
				for (size_t j = 0; j < loop; ++j)
				{
					move = solver();
					if (move)
						solved++;
					else if (verbose && j == 0)
						// 解けなかった時にその問題を出力する。
						cout << "unsolved : line = " << i + 1 << endl <<
						"sfen " << sfen << endl; // このままの形式でどこかに貼り付けたり、やねうら王に局面設定したりできる。

				}
			}

			elapsed = timer.elapsed();
			times = size * loop;
			output_pv();

			cout << test_name << " test end" << endl;

		};

#if defined(USE_MATE_SOLVER)
		// 2進数にしてbit0が立ってたら奇数詰めを呼び出す
		if (test_mode & 1)
			bench([&]() { Mate::MateSolver solver; return solver.mate_odd_ply(pos, mate_ply, true); }, "mate_odd_ply");
#endif

#if defined(USE_MATE_DFPN)
		// 2進数にしてbit1が立ってたらdfpnを呼び出す
		// 速度比較用。
		if (test_mode & 2)
			bench([&]() { auto m = dfpn1.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn1.get_nodes_searched(); return m; }, "mate_dfpn1 : Node32bit");

		if (test_mode & 4)
			bench([&]() { auto m = dfpn2.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn2.get_nodes_searched(); return m; }, "mate_dfpn2 : Node16bitOrdering");

		if (test_mode & 8)
			bench([&]() { auto m = dfpn3.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn3.get_nodes_searched(); return m; }, "mate_dfpn3 : Node48bitOrdering");

		if (test_mode & 16)
		{
			mate_hash.clear(threads);
			bench([&]() { auto m = dfpn4.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn4.get_nodes_searched(); return m; }, "mate_dfpn4 : Node32bitWithHash");
		}

		if (test_mode & 32)
		{
			mate_hash.clear(threads);
			bench([&]() { auto m = dfpn5.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn5.get_nodes_searched(); return m; }, "mate_dfpn5 : Node48bitOrderingWithHash");
		}

#endif

		// 他にも詰将棋ルーチンを追加(or 改良)した時に、ここに追加していく。

#endif // !(defined (USE_MATE_SOLVER) || defined(USE_MATE_DFPN))
	}

	// ----------------------------------
	//      "test mate_dfpn" command
	// ----------------------------------

	// 現在の局面に対してdf-pn詰め将棋ルーチンを呼び出す。
	void mate_dfpn(IEngine& engine, std::istringstream& is)
	{
#if !defined(USE_MATE_DFPN)
		cout << "Error! : define USE_MATE_DFPN" << endl;
		return;
#else
		// 現在の局面
		auto& pos = engine.get_position();

		// default 1000万ノード(どうせ先にメモリが足りなくなる)
		size_t nodes = 10000000;

		// df-pn用メモリ
		size_t mem = 1024;
		size_t dfpn_hash = 1024;

		string token;
		while (is >> token)
		{
			if (token == "nodes")
				is >> nodes;
			else if (token == "mem")
				is >> mem;
			else if (token == "hash")
				is >> dfpn_hash;
		}

		cout << "df-pn mate :" << endl
			 << " nodes = " << nodes << endl
			 << " mem   = " << mem << "[MB]"<< endl
			 << " hash  = " << dfpn_hash << "[MB]" << endl
			;

#if 1
		Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node64bit);
		//Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node48bitOrdering);
#else
		// 置換表を持っているdfpn solver。テスト用。
		Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node48bitOrderingWithHash);
		Mate::MateHashTable mate_hash;
		mate_hash.resize(dfpn_hash);
		mate_hash.clear();
		dfpn.set_hash_table(&mate_hash);
#endif

		dfpn.alloc(mem);

		ElapsedTimer time;
		cout << "start mate." << endl;
		time.reset();
		Move m = dfpn.mate_dfpn(pos, (u32)nodes);
		cout << "time = " << time.elapsed() << endl;
		auto nodes_searched = dfpn.get_nodes_searched();
		if (m != Move::none() && m != Move::null())
		{
			cout << "solved! , nodes_searched = " << nodes_searched << endl;

			auto pv = dfpn.get_pv();
			for (auto m : pv)
				cout << m << " ";

			cout << endl;
		}
		else {
			if (m == Move::null())
				cout << "solved! this is no mate. nodes_searched = " << nodes_searched << endl;
			else if (dfpn.is_out_of_memory())
				cout << "out of memory" << endl;
			else if (dfpn.get_nodes_searched() >= nodes)
				cout << "unsolved , nodes limit." << endl;
			else
				cout << "unsolved. why?" << endl;
		}

#endif
	}

	// ----------------------------------
	//      "test matebench2" command
	// ----------------------------------

	// tanuki-詰将棋ルーチンのbench
	// そのうち削除するかも。


	// MATE ENGINEのテスト。(ENGINEに対して局面図を送信する)
	void mate_bench2(IEngine& engine, [[maybe_unused]] std::istringstream& is)
	{
		cout << "Error! : define TANUKI_MATE_ENGINE or YANEURAOU_MATE_ENGINE" << endl;
	}

} // namespace


// ----------------------------------
//      "test" command Decorator
// ----------------------------------

namespace Test
{
	// 詰み関係のテストコマンド。コマンドを処理した時 trueが返る。
	bool mate_test_cmd(IEngine& engine, std::istringstream& is, const std::string& token)
	{
		if (token == "matebench")  mate_bench(engine,is);       // 詰みルーチンに関するbenchをとる。
		else if (token == "matebench2") mate_bench2(engine,is);      // MATE ENGINEのテスト。(ENGINEに対して局面図を送信する)
		else if (token == "dfpn")       mate_dfpn(engine,is);        // 現在の局面に対してdf-pn詰め将棋ルーチンを呼び出す。
		//else if (token == "matesolve") mate_solve(engine,is);      // 現在の局面に対してN手詰みルーチンを呼び出す。
		else return false;									         // どのコマンドも処理することがなかった
			
		// いずれかのコマンドを処理した。
		return true;
	}

} // namespace Test

} // namespace YaneuraOu

#endif // defined(ENABLE_TEST_CMD)
