#ifndef __STRATEGY_H
#define __STRATEGY_H

#include "common.h"
#include "bidiarray.h"
#include "move.h"
#define DEPTH 6


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

    //! Call this function to save your best move.
    //! Multiple call can be done each turn,
    //! Only the last move saved will be used.
    void (*_saveBestMove)(movement &);

    //! check if the board is full
    bool isBoardFull() const;

public:
    // Constructor from a current situation
    Strategy(bidiarray<Sint16> &blobs,
             const bidiarray<bool> &holes,
             const Uint16 current_player,
             void (*saveBestMove)(movement &));


    // Copy constructor
    Strategy(const Strategy &St)
            : _blobs(St._blobs), _holes(St._holes), _current_player(St._current_player), _availablePosNum(St._availablePosNum)
    {}

    // Destructor
    ~Strategy()
    {}

    /**
     * Apply a move to the current state of blobs
     * Assumes that the move is valid
     */
    void applyMove(const movement &mv);

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

};

#endif
