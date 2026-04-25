#include "strategy.h"
#include <cstdint>
#include <limits>


Strategy::Strategy(bidiarray<Sint16> &blobs,
    const bidiarray<bool> &holes,
    const Uint16 current_player,
    void (*saveBestMove)(movement &))
    : _blobs(blobs), _holes(holes), _current_player(current_player), _saveBestMove(saveBestMove)
{
    _availablePosNum = 0;
    for(Sint8 i = 0; i < 8; i++){
        for(Sint8 j = 0; j < 8; j++){
            if(!_holes.get(i, j) && _blobs.get(i, j) == -1) _availablePosNum++;
        }
    }
#if DEBUG
    cout << "Available position number: " <<_availablePosNum << endl;
#endif
}

bool Strategy::isBoardFull() const{
    return _availablePosNum <= 0;
}


void Strategy::applyMove (const movement& mv) {
    // check either the move is a copy or a move
    if((mv.ox-mv.nx)*(mv.ox-mv.nx)<=1 && (mv.oy-mv.ny)*(mv.oy-mv.ny)<=1)
    { // copy
        _blobs.set(mv.nx, mv.ny, _current_player);
        _availablePosNum--; // blob duplication decrease available position number;
    }else{ // move
        _blobs.set(mv.ox, mv.oy, -1);
        _blobs.set(mv.nx, mv.ny, _current_player);
    }

    // check for neighbors change
    for(Sint8 i = -1; i < 2; i++){
        for(Sint8 j = -1; j < 2; j++){
            if(mv.nx+i < 0 || mv.nx+i > 7) continue;
            if(mv.ny+j < 0 || mv.ny+j > 7) continue;
            if(_blobs.get(mv.nx+i, mv.ny+j) != -1 && _blobs.get(mv.nx+i, mv.ny+j) != _current_player){
                _blobs.set(mv.nx+i, mv.ny+j, _current_player);
            }
        }
    }
    switchPlayer(); // once move is applying, we change the current player
}

Sint32 Strategy::estimateCurrentScore () const {
    Sint32 score = 0;
    for(int i = 0; i < 8; i++){
        for(int j = 0; j < 8; j++){
            if(_blobs.get(i,j) != -1){
                if(_blobs.get(i,j) == (int) _current_player) score++;
                else score--;
            }
        }
    }
    if(score > 0 && isBoardFull()) return 100;

    return score;
}

vector<movement>& Strategy::computeValidMoves (vector<movement>& valid_moves) const {
    movement mv(0,0,0,0);
    //iterate on starting position
    for(mv.ox = 0 ; mv.ox < 8 ; mv.ox++) {
        for(mv.oy = 0 ; mv.oy < 8 ; mv.oy++) {
            if (_blobs.get(mv.ox, mv.oy) == (int) _current_player) {
                //iterate on possible destinations
                for(mv.nx = std::max(0,mv.ox-2) ; mv.nx <= std::min(7,mv.ox+2) ; mv.nx++) {
                    for(mv.ny = std::max(0,mv.oy-2) ; mv.ny <= std::min(7,mv.oy+2) ; mv.ny++) {
                        if (_holes.get(mv.nx, mv.ny)) continue;
                        if (_blobs.get(mv.nx, mv.ny) == -1) valid_moves.push_back(movement(mv));
                    }
                }
            }
        }
    }
    return valid_moves;
}

void Strategy::switchPlayer(){
    _current_player = (_current_player+1)&1;
}

/*
 *   ====== implementation 1 profondeur simple ==========
 void Strategy::computeBestMove () {
    Sint32 bestScore = numeric_limits<Sint32>::min();
    movement bestMove;
    vector<movement> valid_moves;
    computeValidMoves(valid_moves);

    for(movement &mv : valid_moves){
        Strategy nextStrat = Strategy(*this);
        nextStrat.applyMove(mv);
        Sint32 score = nextStrat.estimateCurrentScore();
        if(score>bestScore){
            bestScore = score;
            bestMove = mv;
        }
    }
    _saveBestMove(bestMove);
    return;
}
*/

/*
 *  ======    implement minMax algo with negamax convention
 */
Sint32 Strategy::negamax(int depth, movement &bestMove){
    if(depth <= 0 || isBoardFull())
        return estimateCurrentScore();

    Sint32 bestScore = numeric_limits<Sint32>::min(); // -INFINITY
    vector<movement> valid;
    computeValidMoves(valid); // search all possible validMoves

    if(valid.empty()){
        // no valid move, so we skip the current player
        Strategy next(*this);
        next.switchPlayer();
        movement dummy;
        return -next.negamax(depth-1, dummy);
    }
    movement localBest;

    for(movement& mv: valid){
        Strategy next(*this);
        next.applyMove(mv);
        movement dummy;
        Sint32 score = -next.negamax(depth-1, dummy);
        if(score > bestScore){
            bestScore = score;
            localBest = mv;
        }
    }
    bestMove = localBest;
    return bestScore;
}

Sint32 Strategy::alphaBetaSeq(int depth, Sint32 alpha, Sint32 beta, movement& bestMove){
    if(depth <= 0 || isBoardFull())
        return estimateCurrentScore();

    vector<movement> valid;
    computeValidMoves(valid); // search all possible validMoves
    movement dummy;
    if(valid.empty()){
        // no valid move, so we skip the current player
        Strategy next(*this);
        next.switchPlayer();
        return -next.alphaBetaSeq(depth-1, -beta, -alpha, dummy);
    }
    for(movement& mv: valid){
        Strategy next(*this);
        next.applyMove(mv);
        int score = -next.alphaBetaSeq(depth-1, -beta, -alpha, dummy);
        if(score>=alpha){
#if DEBUG
            //cout<<"coupure"<< score<<">=" << alpha<<endl;
#endif
            alpha = score;
            bestMove = mv;
            if(alpha>=beta)
                break;
        }
    }
    return alpha;
}

void Strategy::computeBestMove () {

    movement bestMove;
    cout << "score: " << this->alphaBetaSeq(DEPTH, numeric_limits<Sint32>::min(), numeric_limits<Sint32>::max(), bestMove) << endl;
    _saveBestMove(bestMove);
}

