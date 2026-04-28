#ifndef __STRATEGY_H
#define __STRATEGY_H

#include "common.h"
#include "bidiarray.h"
#include "move.h"
#include "random"
#include <algorithm>
#define DEPTH 6

enum class TTFlag : uint8_t { EXACT, LOWER, UPPER };

struct TTEntry {
    uint64_t hash;
    Sint32   score;
    int      depth;
    TTFlag   flag;
    struct movement mv;
};

struct Weights {
    int material;    // #blobs
    int frontier;    // différentiel frontier ouverte
    int center;      // masse centrale
    int danger;      // pénalité frontier critique
};

struct PlayerStats {
    int material;
    int center;
    int frontier;
    int min_border;
};


class Strategy {

private:
    //! array containing all blobs on the board
    bidiarray<Sint16> _blobs;
    //! an array of booleans indicating for each cell whether it is a hole or not.
    const bidiarray<bool> &_holes;
    //! Current player
    Uint16 _current_player;

    //! current number of available position
    Sint16 _availablePosNum;
    Sint16 _initalAvailablePosNum;
    //! flag to know if the table is alredy set
    static bool _isZobristHashesSet;
    //! tableau des hash de zobrist pour la table
    static uint64_t _zobristHashes[64][3] ;
    static uint64_t _zobrist_turn; // hash for the player switching
    //! transposition table size
    static constexpr size_t TT_SIZE = 1 << 22;  // ~4 millions entries
    static struct TTEntry _TT[TT_SIZE];

    //! Indexage :takes the N least significant bits from hash
    size_t tt_index(uint64_t hash) {
        return hash & (TT_SIZE - 1);
    }
    //! Lookup
    TTEntry* tt_lookup(uint64_t hash) {
        TTEntry& entry = _TT[tt_index(hash)];
        if (entry.hash == hash) return &entry;
        return nullptr;
    }
    //! Store
    void tt_store(uint64_t hash, Sint32 score, int depth, TTFlag flag, struct movement best_move) {
        TTEntry& entry = _TT[tt_index(hash)];
        // depth-first:
        if (entry.hash == 0 || depth >= entry.depth) {
            entry = {hash, score, depth, flag, best_move};
        }
    }

    //! current zobrist hash
    uint64_t _currZobristHash;

    //! Call this function to save your best move.
    //! Multiple call can be done each turn,
    //! Only the last move saved will be used.
    void (*_saveBestMove)(movement &);

    //! check if the board is full
    bool isBoardFull() const;

    const Weights& select_weights(Sint16 available, Sint16 initial)const;

    void collect_stats(int player, PlayerStats& out)const;

    //! helper to sort movement
    int scoreMove(const movement& mv);

    void sortMove(vector<movement>& valid);


public:
    // Constructor from a current situation
    Strategy(bidiarray<Sint16> &blobs,
             const bidiarray<bool> &holes,
             const Uint16 current_player,
             void (*saveBestMove)(movement &));


    // Copy constructor
    Strategy(const Strategy &St)
            : _blobs(St._blobs), _holes(St._holes), _current_player(St._current_player), _availablePosNum(St._availablePosNum),
              _initalAvailablePosNum(St._initalAvailablePosNum), _currZobristHash(St._currZobristHash)
    {}

    // Destructor
    ~Strategy()
    {}

    /**
     * Apply a move to the current state of blobs
     * Assumes that the move is valid
     */
    moveInfo applyMove(const movement &mv);
    /**
    * Apply a move to the current state of blobs
    * Assumes that the move is valid
    */
    void undoMove(const moveInfo &info);

    /**
     * Compute the vector containing every possible moves
     */
    vector <movement> &computeValidMoves(vector <movement> &valid_moves) const;

    /**
     * Estimate the score of the current state of the game
     */
    Sint32 estimateCurrentScore() const;

    /**
     * Find the best move.
     */
    void computeBestMove();

    /**
     *  implement negamax algorithme to find the best move to play with max DEPTH search
     */
    Sint32 negamax(int depth, movement &bestMove);

    /**
     *  switch current player
     */
    void switchPlayer();

    /**
     * implement alpha beta algorithme to find the next bestMove with max depth search (seq algo)
     */
    Sint32 alphaBetaSeq(int depth, Sint32 alpha, Sint32 beta, movement &bestMove);

    /**
     * reset TT and zobrist hashes table
     */
     static void reset();

};

#endif
