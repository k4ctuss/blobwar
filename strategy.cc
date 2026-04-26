#include "strategy.h"
#include <cstdint>
#include <limits>


// def of class attributs
uint64_t Strategy::_zobristHashes[64][3];
uint64_t Strategy::_zobrist_turn;
TTEntry  Strategy::_TT[Strategy::TT_SIZE];
bool Strategy::_isZobristHashesSet = false;

void Strategy::reset(){
#if DEBUG
    cout<< "reset de Strategy"<<endl;
#endif
    _isZobristHashesSet = false;
    std::fill(std::begin(_TT), std::end(_TT), TTEntry{});
}

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
    _initalAvailablePosNum = _availablePosNum;

    std::mt19937_64 rng(12345678ULL);
    if(!_isZobristHashesSet){
        _isZobristHashesSet = true;
        for(Sint8 i = 0; i < 8; i++){
            for(Sint8 j = 0; j < 8; j++){
                for(int state=0; state<3; state++){
                    _zobristHashes[i*8+j][state] = rng();
                }
            }
        }
        _zobrist_turn = rng();
    }


    _currZobristHash = 0;
    for(Sint8 i = 0; i < 8; i++){
        for(Sint8 j = 0; j < 8; j++){
            if(_holes.get(i, j)){
                _currZobristHash ^= _zobristHashes[i*8+j][2];
            }else{
                if(_blobs.get(i, j) != -1){
                    _currZobristHash ^= _zobristHashes[i*8+j][_blobs.get(i, j)];
                }
            }
        }
    }
    if(_current_player == 1) _currZobristHash ^= _zobrist_turn;

#if DEBUG
    cout << "Available position number: " <<_availablePosNum << endl;
#endif
}

bool Strategy::isBoardFull() const{
    if(_availablePosNum <= 0)return true;
    return false;
}


moveInfo Strategy::applyMove (const movement& mv) {

    moveInfo info;
    info.mv = mv;
    info.converted_count = 0;
    int ddx = mv.ox - mv.nx, ddy = mv.oy - mv.ny;
    info.isCopy = (ddx*ddx <= 1 && ddy*ddy <= 1);

    // check either the move is a copy or a move
    if(info.isCopy)
    { // copy
        _blobs.set(mv.nx, mv.ny, _current_player);
        _availablePosNum--; // blob duplication decrease available position number;
        _currZobristHash ^= _zobristHashes[mv.nx*8+mv.ny][_current_player];
    }else{ // move
        _blobs.set(mv.ox, mv.oy, -1);
        _blobs.set(mv.nx, mv.ny, _current_player);
        _currZobristHash ^= _zobristHashes[mv.nx*8+mv.ny][_current_player];
        _currZobristHash ^= _zobristHashes[mv.ox*8+mv.oy][_current_player];
    }

    // check for neighbors change
    for(Sint8 i = -1; i < 2; i++){
        for(Sint8 j = -1; j < 2; j++){
            Sint8 keyX = mv.nx+i, keyY = mv.ny+j;
            if(keyX < 0 || keyX > 7) continue;
            if(keyY < 0 || keyY > 7) continue;
            if(_blobs.get(keyX, keyY) != -1 && _blobs.get(keyX,keyY) != _current_player){

                //save info
                info.converted_x[info.converted_count] = keyX;
                info.converted_y[info.converted_count] = keyY;
                info.converted_count++;

                _blobs.set(keyX, keyY, _current_player);
                _currZobristHash ^= _zobristHashes[keyX*8+keyY][_current_player]; // add play
                _currZobristHash ^= _zobristHashes[keyX*8+keyY][1-_current_player]; // remove opponent
            }
        }
    }
    switchPlayer(); // once move is applying, we change the current player
    _currZobristHash ^= _zobrist_turn;
    return info;
}

void Strategy::undoMove(const moveInfo &info){
    // on revient au joueur qui a joué ce coup
    switchPlayer();
    _currZobristHash ^= _zobrist_turn;

    movement mv = info.mv;

    if(info.isCopy){
        _blobs.set(mv.nx, mv.ny, -1);
        _availablePosNum++;
        _currZobristHash ^= _zobristHashes[mv.nx*8+mv.ny][_current_player];
    }else{
        _blobs.set(mv.ox, mv.oy, _current_player);
        _blobs.set(mv.nx, mv.ny, -1);
        _currZobristHash ^= _zobristHashes[mv.nx*8+mv.ny][_current_player];
        _currZobristHash ^= _zobristHashes[mv.ox*8+mv.oy][_current_player];
    }

    Uint16 opp = 1-_current_player;
    for(Uint8 i = 0; i < info.converted_count; i++){
        Sint8 kx = info.converted_x[i];
        Sint8 ky = info.converted_y[i];
        _blobs.set(kx, ky, 1-_current_player);
        _currZobristHash ^= _zobristHashes[kx*8+ky][_current_player];
        _currZobristHash ^= _zobristHashes[kx*8+ky][opp];
    }
}

/*
 * **************************************
 * ****  Start estimation section :  ****
 */

// directions 4-connexes
static constexpr int DX[4] = { 1, -1,  0,  0 };
static constexpr int DY[4] = { 0,  0,  1, -1 };

static inline bool on_board(int x, int y) {
    return (unsigned)x < 8u && (unsigned)y < 8u;
}

static constexpr int WINNING_SCORE = 100000;

// weight selection

static constexpr Weights W_EARLY { 150,  60,  80, 25 };
static constexpr Weights W_MID   { 200,  90, 130, 12 };
static constexpr Weights W_LATE  { 250,  70, 200,  5 };

BlobStats Strategy::bfs(int player) const{
    int sx = -1, sy = -1;
    for (int x = 0; x < 8 && sx < 0; ++x)
        for (int y = 0; y < 8 && sx < 0; ++y)
            if (_blobs.get(x, y) == player) { sx = x; sy = y; }

    BlobStats s{ 0, 0, 0, false };
    if (sx < 0) return s;

    uint64_t vis_blob   = 0;
    uint64_t vis_border = 0;

    int qx[64], qy[64];
    int head = 0, tail = 0;

    auto push = [&](int x, int y) {
        uint64_t bit = 1ULL << (y * 8 + x);
        if (vis_blob & bit) return;
        vis_blob |= bit;
        qx[tail] = x; qy[tail] = y;
        tail = (tail + 1) & 63;
    };

    push(sx, sy);
    int open_adj_total = 0;

    while (head != tail) {
        int cx = qx[head], cy = qy[head];
        head = (head + 1) & 63;
        ++s.size;

        int dist = std::max(std::abs(cx - 3), std::abs(cy - 3));
        s.center += std::max(0, 3 - dist);

        for (int d = 0; d < 4; ++d) {
            int nx = cx + DX[d];
            int ny = cy + DY[d];
            if (!on_board(nx, ny) || _holes.get(nx, ny)) continue;

            Sint16 cell = _blobs.get(nx, ny);
            if (cell == player) {
                push(nx, ny);
            } else if (cell == -1) {
                ++open_adj_total;
                uint64_t bit = 1ULL << (ny * 8 + nx);
                if (!(vis_border & bit)) {
                    vis_border |= bit;
                    ++s.border;
                }
            }
        }
    }

    s.contained = (open_adj_total == 0);
    return s;
}

const Weights& Strategy::select_weights() const {
    if (_initalAvailablePosNum == 0) return W_LATE;

    if (_availablePosNum * 100 < _initalAvailablePosNum * 35) return W_EARLY;
    if (_availablePosNum * 100 < _initalAvailablePosNum * 65) return W_MID;
    return W_LATE;
}
/*
Sint32 Strategy::estimateCurrentScore() const {
    const int me  = static_cast<int>(_current_player);
    const int opp = 1 - me;

    const Weights& W = select_weights();

    BlobStats my  = bfs(me);
    BlobStats adv = bfs(opp);

    if (my.contained)  return -WINNING_SCORE + my.size;
    if (adv.contained) return  WINNING_SCORE - adv.size;

    Sint32 score = W.blob * (my.size - adv.size);

    score += W.border * (my.border - adv.border);

    if (my.border <= 2)
        score -= W.contained * (3 - my.border);
    if (adv.border <= 2)
        score += W.contained * (3 - adv.border);

    score += W.center * (my.center - adv.center);

    return score;
}
*/

Sint32 Strategy::estimateCurrentScore() const {

    Sint32 myCount = 0;
    Sint32 oppCount = 0;

    Sint32 myMobility = 0;
    Sint32 oppMobility = 0;

    Sint32 myPotential = 0;
    Sint32 oppPotential = 0;

    Sint32 myCloneSpots = 0;
    Sint32 oppCloneSpots = 0;

    Sint32 myIsolated = 0;
    Sint32 oppIsolated = 0;

    Uint16 opponent = 1-_current_player;

    static const int dx[8] = {-1,-1,-1,0,0,1,1,1};
    static const int dy[8] = {-1,0,1,-1,1,-1,0,1};

    int width = 8;
    int height = 8;

    // Use bitsets to track unique empty and opponent cells (8x8 board = 64 cells max)
    uint64_t myCountedEmpty = 0;
    uint64_t myCountedOpponent = 0;
    uint64_t oppCountedEmpty = 0;
    uint64_t oppCountedOpponent = 0;

    for(int x = 0; x < width; x++) {
        for(int y = 0; y < height; y++) {

            if(_holes.get(x,y)) continue;

            Sint16 cell = _blobs.get(x,y);

            if(cell == _current_player) {

                myCount++;

                bool hasEmptyNeighbor = false;

                for(int d = 0; d < 8; d++) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];

                    if(nx < 0 || ny < 0 || nx >= width || ny >= height)
                        continue;

                    if(_holes.get(nx, ny)) continue;

                    Sint16 neighbor = _blobs.get(nx, ny);
                    uint64_t cellBit = 1ULL << (ny * 8 + nx);

                    if(neighbor == 0) {
                        hasEmptyNeighbor = true;
                        // Only count each unique empty cell once
                        if(!(myCountedEmpty & cellBit)) {
                            myCountedEmpty |= cellBit;
                            myCloneSpots++;
                        }
                    }

                    if(neighbor == opponent) {
                        // Only count each unique opponent cell once
                        if(!(myCountedOpponent & cellBit)) {
                            myCountedOpponent |= cellBit;
                            myPotential++;
                        }
                    }
                }

                if(!hasEmptyNeighbor)
                    myIsolated++; // forced to jump

            } else if(cell == opponent) {

                oppCount++;

                bool hasEmptyNeighbor = false;

                for(int d = 0; d < 8; d++) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];

                    if(nx < 0 || ny < 0 || nx >= width || ny >= height)
                        continue;

                    if(_holes.get(nx, ny)) continue;

                    Sint16 neighbor = _blobs.get(nx, ny);
                    uint64_t cellBit = 1ULL << (ny * 8 + nx);

                    if(neighbor == 0) {
                        hasEmptyNeighbor = true;
                        // Only count each unique empty cell once
                        if(!(oppCountedEmpty & cellBit)) {
                            oppCountedEmpty |= cellBit;
                            oppCloneSpots++;
                        }
                    }

                    if(neighbor == _current_player) {
                        // Only count each unique player cell once
                        if(!(oppCountedOpponent & cellBit)) {
                            oppCountedOpponent |= cellBit;
                            oppPotential++;
                        }
                    }
                }

                if(!hasEmptyNeighbor)
                    oppIsolated++;
            }
        }
    }

    // Mobility
    vector<movement> moves;
    computeValidMoves(moves);
    myMobility = moves.size();

    Strategy tmp(*this);
    tmp.switchPlayer();
    vector<movement> oppMoves;
    tmp.computeValidMoves(oppMoves);
    oppMobility = oppMoves.size();
    if(oppMoves.empty()) {
        // opponent can't play → VERY GOOD
        return 500000;
    }

    // Game phase (0 = endgame, 1 = opening)
    float phase = (float)_availablePosNum / (float)_initalAvailablePosNum;

    // Weights (tunable)
    Sint32 materialWeight = 100;
    Sint32 mobilityWeight = (phase > 0.5 ? 25 : 8);
    Sint32 potentialWeight = 12;
    Sint32 cloneWeight = (phase > 0.4 ? 18 : 8);      // cloning more valuable early
    Sint32 isolationPenalty = 30;                    // strong penalty

    Sint32 score =
            materialWeight * (myCount - oppCount) +
            mobilityWeight * (myMobility - oppMobility) +
            potentialWeight * (myPotential - oppPotential) +
            cloneWeight * (myCloneSpots - oppCloneSpots) -
            isolationPenalty * (myIsolated - oppIsolated);

    return score;
}

/*
 * ****  end estimation section :  ****
 * **************************************
 */

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

int Strategy::scoreMove(const movement& mv, bool oppIsBlocked) {
    int score = 0;

    int dx_move = mv.ox - mv.nx;
    int dy_move = mv.oy - mv.ny;
    bool isCopy = (dx_move * dx_move <= 1 && dy_move * dy_move <= 1);

    // 1. Base scoring: favor clones (distance 1)
    if(isCopy) {
        score += 100;
    } else {
        score += 10;
    }

    // 2. When opponent is blocked, heavily penalize jumps and reward copies
    if(oppIsBlocked) {
        if(isCopy) {
            score += 500;  // Strong bonus for copies when opponent stuck
        } else {
            score -= 300;  // Strong penalty for jumps when opponent stuck
        }
    }

    // 3. Count how many opponent pieces will be flipped
    int flipped = 0;

    static const int dx[8] = {-1,-1,-1,0,0,1,1,1};
    static const int dy[8] = {-1,0,1,-1,1,-1,0,1};

    Uint16 opponent = 1-_current_player;

    for(int d = 0; d < 8; d++) {
        int nx = mv.nx + dx[d];
        int ny = mv.ny + dy[d];

        if(nx < 0 || ny < 0 || nx >= 8 || ny >= 8)
            continue;

        if(_holes.get(nx, ny)) continue;

        if(_blobs.get(nx, ny) == opponent)
            flipped++;
    }

    score += flipped * 50;

    return score;
}

void Strategy::switchPlayer(){
    _current_player = 1-_current_player;
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

    // Check if opponent is blocked (compute once, not for each move)
    switchPlayer();
    vector<movement> oppCheckMoves;
    computeValidMoves(oppCheckMoves);
    switchPlayer();
    bool currentOppBlocked = oppCheckMoves.empty();

    sort(valid.begin(), valid.end(), [&](const movement& a, const movement& b) {
        return scoreMove(a, currentOppBlocked) > scoreMove(b, currentOppBlocked);
    });
    for(movement& mv: valid){
        moveInfo info = applyMove(mv);
        movement dummy;
        Sint32 score = -negamax(depth-1, dummy);
        undoMove(info);
        if(score > bestScore){
            bestScore = score;
            localBest = mv;
        }
    }
    bestMove = localBest;
    return bestScore;
}

Sint32 Strategy::alphaBetaSeq(int depth, Sint32 alpha, Sint32 beta, movement& bestMove){

    Sint32 initAlpha = alpha;
    // check TT
    struct TTEntry* entry = tt_lookup(_currZobristHash);
    if(entry != nullptr && entry->depth >= depth){
#if DEBUG
        //cout << "TT hit"<< endl;
#endif
        if (entry->flag == TTFlag::EXACT) return entry->score;
        if (entry->flag == TTFlag::LOWER) alpha = std::max(alpha, entry->score);
        if (entry->flag == TTFlag::UPPER) beta  = std::min(beta,  entry->score);
        if (alpha >= beta) return entry->score;
    }

    if(depth <= 0 || isBoardFull())
        return estimateCurrentScore();

    vector<movement> valid;
    computeValidMoves(valid); // search all possible validMoves
    movement dummy;
    if(valid.empty()){
        switchPlayer();
        return -alphaBetaSeq(depth-1, -beta, -alpha, dummy);
    }

    // Check if opponent is blocked (compute once, not for each move)
    switchPlayer();
    vector<movement> oppCheckMoves;
    computeValidMoves(oppCheckMoves);
    bool currentOppBlocked = oppCheckMoves.empty();
    switchPlayer();

    sort(valid.begin(), valid.end(), [&](const movement& a, const movement& b) {
        return scoreMove(a, currentOppBlocked) > scoreMove(b, currentOppBlocked);
    });
    Sint32 bestScore = numeric_limits<Sint32>::min(); // -INFINITY
    for(movement& mv: valid){
        moveInfo info = applyMove(mv);
        int score = -alphaBetaSeq(depth-1, -beta, -alpha, dummy);
        undoMove(info);
        if(score > bestScore) {
            bestScore = score;
            bestMove = mv;
        }
        if(score > alpha){
            alpha = score;
        }
        if(alpha>=beta){
#if DEBUG
            //cout<<"coupure"<< score<<">=" << alpha<<endl;
#endif
            break;
        }
    }

    TTFlag flag;
    if(bestScore <= initAlpha)
        flag = TTFlag::UPPER;   // aucun coup n'a amélioré alpha → borne haute
    else if(bestScore >= beta)
        flag = TTFlag::LOWER;   // coupure bêta → borne basse
    else
        flag = TTFlag::EXACT;   // valeur exacte dans la fenêtre

    tt_store(_currZobristHash, bestScore, depth, flag, bestMove);
    return bestScore;
}

void Strategy::computeBestMove () {

    movement bestMove;
    cout << "score: " << this->alphaBetaSeq(DEPTH, numeric_limits<Sint32>::min(), numeric_limits<Sint32>::max(), bestMove) << endl;
    _saveBestMove(bestMove);
}

