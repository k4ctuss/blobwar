#include "strategy.h"
#include <cstdint>
#include <limits>

/**
 * mes pensées:
 * ammelioration a faire si j'étais fort:
 * - quand adversaire ne peux plus bouger, si tout meme a defaite, aller cherhcer la draw avec des jumps aller-retour
 * - quand adversaire ne peux plus bouger, si 1 chemin meme a la victoire, prendre direct
 * - quand trouve victoire il faut break et ne pas chercher plus de pronfondeur.
 * - killer heuristic and it's generalization the history heuristic
 *
 * Structure de donnée :
 *  - utiliser des uint64 pour representer blob car bit a 1 => blob present sinon absent (heuristique en 0(1) avec popcount
 */


/**
 * bilan des test:
 *  - alpha beta avec et sans TT fond les meme perf en match. victoire blue 34-30 peut importe si blue a TT ou non
 *  - alphabeta vs minmax(profondeur 4 pour minmax sinon timeout)
 */

// def of class attributs
uint64_t Strategy::_zobristHashes[64][3];
uint64_t Strategy::_zobrist_turn;
TTEntry  Strategy::_TT[Strategy::TT_SIZE];
bool Strategy::_isZobristHashesSet = false;
static constexpr Sint32 INF = 1000000;

void Strategy::reset(){
#if DEBUG
    cout<< "reset de Strategy"<<endl;
#endif
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
static constexpr int DX4[4] = { 1, -1,  0,  0 };
static constexpr int DY4[4] = { 0,  0,  1, -1 };

static constexpr int CENTER_BONUS[8][8] = {
        {0,0,0,0,0,0,0,0},
        {0,1,1,1,1,1,1,0},
        {0,1,2,2,2,2,1,0},
        {0,1,2,3,3,2,1,0},
        {0,1,2,3,3,2,1,0},
        {0,1,2,2,2,2,1,0},
        {0,1,1,1,1,1,1,0},
        {0,0,0,0,0,0,0,0},
};

static inline bool on_board(int x, int y) {
    return (unsigned)x < 8u && (unsigned)y < 8u;
}


// weight selection
static constexpr int WINNING_SCORE = 100000;
static constexpr Weights W_EARLY{ 300,   80,     40,     60 };
static constexpr Weights W_MID  { 300,  100,     20,    120 };
static constexpr Weights W_LATE { 300,   60,      5,    200 };


const Weights &Strategy::select_weights(Sint16 available, Sint16 initial)const{
    if (initial == 0) return W_LATE;
    if (available * 100 > initial * 65) return W_EARLY;
    if (available * 100 > initial * 35) return W_MID;
    return W_LATE;
}

void Strategy::collect_stats(int player, PlayerStats& out)const{
    out = {0, 0, 0, 8}; // min_border initialisé à 8 (max possible)

    uint64_t frontier_mask = 0; // bitmap des cases frontier

    for (int x = 0; x < 8; ++x) {
        for (int y = 0; y < 8; ++y) {
            if (_blobs.get(x, y) != player) continue;

            // material + centre
            out.material++;
            out.center += CENTER_BONUS[x][y];

            // frontier locale de cette case
            int local_free = 0;
            for (int d = 0; d < 4; ++d) {
                int nx = x + DX4[d];
                int ny = y + DY4[d];
                if ((unsigned)nx >= 8u || (unsigned)ny >= 8u) continue;
                if (_holes.get(nx, ny)) continue;
                if (_blobs.get(nx, ny) == -1) {
                    ++local_free;
                    frontier_mask |= 1ULL << (nx * 8 + ny);
                }
            }

            // min_border = cas le plus critique
            if (local_free < out.min_border)
                out.min_border = local_free;
        }
    }

    // popcount du bitmap = frontier totale distincte
    out.frontier = __builtin_popcountll(frontier_mask);

    // si aucun blob : contained
    if (out.material == 0) out.min_border = 0;
}
/*
 *
Sint32 Strategy::estimateCurrentScore() const {
    const int me  = _current_player;
    const int opp = 1 - me;

    PlayerStats my, adv;
    collect_stats(me, my);
    collect_stats(opp, adv);


    if (my.frontier  == 0 && my.material  > 0) return -WINNING_SCORE;
    if (adv.frontier == 0 && adv.material > 0) return  WINNING_SCORE;

    const Weights& W = select_weights(_availablePosNum, _initalAvailablePosNum);


    Sint32 score = 0;

    score += W.material * (my.material - adv.material);
    score += W.frontier * (my.frontier - adv.frontier);
    score += W.center * (my.center - adv.center);

    if (my.min_border  <= 2)
        score -= W.danger * (3 - my.min_border);
    if (adv.min_border <= 2)
        score += W.danger * (3 - adv.min_border);

    return score;
}
*/

Sint32 Strategy::estimateCurrentScore() const {
    Sint32 score = 0;
    for(int i = 0; i < 8; i++){
        for(int j = 0; j < 8; j++){
            if(_blobs.get(i,j) != -1){
                if(_blobs.get(i,j) == (int) _current_player) score++;
                else score--;
            }
        }
    }
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

int Strategy::scoreMove(const movement& mv) {
    int dx_move = mv.ox - mv.nx;
    int dy_move = mv.oy - mv.ny;
    bool isCopy = (dx_move * dx_move <= 1 && dy_move * dy_move <= 1);

    // copie >> saut, toujours
    int score = isCopy ? 100 : 10;

    // captures : terme le plus discriminant après copy/jump
    static const int dx[8] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const int dy[8] = {-1, 0, 1,-1, 1,-1, 0, 1};
    int opp = 1 - _current_player;

    for (int d = 0; d < 8; d++) {
        int nx = mv.nx + dx[d];
        int ny = mv.ny + dy[d];
        if ((unsigned)nx >= 8u || (unsigned)ny >= 8u) continue;
        if (_holes.get(nx, ny)) continue;
        if (_blobs.get(nx, ny) == opp) score += 50;
    }

    return score;
}

void Strategy::sortMove(vector <movement> &valid){
    // Précalcul unique de chaque score — scoreMove appelé exactement une fois par coup
    int n = valid.size();
    vector<int> scores(n);
    for (int i = 0; i < n; i++)
        scores[i] = scoreMove(valid[i]);

    // Tri par insertion — rapide pour n < 20, ce qui est souvent le cas
    for (int i = 1; i < n; i++) {
        movement mv = valid[i];
        int sc = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < sc) {
            valid[j+1]  = valid[j];
            scores[j+1] = scores[j];
            --j;
        }
        valid[j+1]  = mv;
        scores[j+1] = sc;
    }
}

void Strategy::switchPlayer(){
    _current_player = 1-_current_player;
}

/*
 *   ====== implementation 1 profondeur simple ==========
 void Strategy::computeBestMove () {
    Sint32 bestScore = -INF;
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
 *  ======    implement minMax algo with negamax form
 */
Sint32 Strategy::negamax(int depth, movement &bestMove){
    if(depth <= 0 || isBoardFull())
        return estimateCurrentScore();

    Sint32 bestScore = -INF; // -INFINITY
    vector<movement> valid;
    computeValidMoves(valid); // search all possible validMoves

    if(valid.empty()){
        // no valid move, so we skip the current player
        switchPlayer();
        movement dummy;
        Sint32 score = -negamax(depth-1, dummy);
        switchPlayer();
        return score;
    }

    for(movement& mv: valid){
        moveInfo info = applyMove(mv);
        movement dummy;
        Sint32 score = -negamax(depth-1, dummy);
        undoMove(info);
        if(score > bestScore){
            bestScore = score;
            bestMove = mv;
        }
    }
    return bestScore;
}

Sint32 Strategy::alphaBetaSeq(int depth, Sint32 alpha, Sint32 beta){

    if(depth == 0 || isBoardFull())
        return estimateCurrentScore();

    vector<movement> valid;
    computeValidMoves(valid); // search all possible validMoves
    //cout<< "curr depth " << depth<<", max depth " << maxDepth<< ", valid length "<< valid.size()<< endl;

    if(valid.empty()){
        switchPlayer();
        Sint32 score = -alphaBetaSeq(depth-1, -beta, -alpha);
        switchPlayer();
        return score;
    }

    Sint32 bestScore = -INF; // -INFINITY
    for(movement& mv: valid){
        moveInfo info = applyMove(mv);
        int score = -alphaBetaSeq(depth-1, -beta, -alpha);
        undoMove(info);
        if(score > bestScore) {
            bestScore = score;
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

    return bestScore;
}

Sint32 Strategy::alphaBetaSeqWithTT(int depth, Sint32 alpha, Sint32 beta){

    if(depth == 0 || isBoardFull())
        return estimateCurrentScore();

    Sint32 initAlpha = alpha;
    // check TT
    struct TTEntry* entry = tt_lookup(_currZobristHash, depth);
    if(entry != nullptr){
#if DEBUG
        //cout << "TT hit"<< endl;
#endif
        if (entry->flag == TTFlag::EXACT) return entry->score;
        if (entry->flag == TTFlag::LOWER) alpha = std::max(alpha, entry->score);
        if (entry->flag == TTFlag::UPPER) beta  = std::min(beta,  entry->score);
        if (alpha >= beta) return entry->score;
    }

    vector<movement> valid;
    computeValidMoves(valid); // search all possible validMoves
    //cout<< "curr depth " << depth<<", max depth " << maxDepth<< ", valid length "<< valid.size()<< endl;

    if(valid.empty()){
        switchPlayer();
        Sint32 score = -alphaBetaSeqWithTT(depth-1, -beta, -alpha);
        switchPlayer();
        // Stocker dans le TT avant de retourner
        tt_store(_currZobristHash, score, depth, TTFlag::EXACT, movement{65,65,65,65});
        return score;
    }

    if(entry){
        movement tt_move = entry->mv;
        // mettre le coup TT en premier — c'est lui qui génère le plus de coupures
        auto it = find_if(valid.begin(), valid.end(), [&](const movement& m) {
            return m.ox == tt_move.ox && m.oy == tt_move.oy
                   && m.nx == tt_move.nx && m.ny == tt_move.ny;
        });
        if (it != valid.end())
            iter_swap(it, valid.begin()); // O(1), pas de copie
    }

    //sortMove(valid);
    movement bestMove = valid[0];
    Sint32 bestScore = -INF; // -INFINITY
    for(movement& mv: valid){
        moveInfo info = applyMove(mv);
        int score = -alphaBetaSeqWithTT(depth-1, -beta, -alpha);
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

Sint32 Strategy::negamaxPar(int depth, movement &bestMove){
    if(depth <= 0 || isBoardFull())
        return estimateCurrentScore();

    Sint32 bestScore = -INF;
    vector<movement> valid;
    computeValidMoves(valid);

    if(valid.empty()){
        Strategy next(*this);
        next.switchPlayer();
        movement dummy;
        return -next.negamaxPar(depth-1, dummy);
    }

    if(depth <= 2){
        for(movement& mv: valid){
            moveInfo info = applyMove(mv);
            movement dummy;
            Sint32 score = -negamax(depth-1, dummy);
            undoMove(info);
            if(score > bestScore){
                bestScore = score;
                bestMove = mv;
            }
        }
        return bestScore;
    }
    
    // Lancer chaque coup en parallèle
    vector<std::future<Sint32>> futures;
    
    for(const movement& mv: valid){
        auto fut = std::async(std::launch::async, [this, mv, depth](){
            Strategy copy(*this);
            copy.applyMove(mv);  // Pas besoin de undoMove !
            movement dummy;
            return -copy.negamaxPar(depth-1, dummy);
        });
        futures.push_back(move(fut));
    }
    
    // Récupérer les résultats
    for(int i = 0; i < (int)futures.size(); i++){
        Sint32 score = futures[i].get();
        if(score > bestScore){
            bestScore = score;
            bestMove = valid[i];
        }
    }
    
    return bestScore;
}



void Strategy::computeBestMove () {

    if(_current_player == 0){ // red on minmax
        movement bestMove;
        negamaxPar(DEPTH, bestMove);
        _saveBestMove(bestMove);

    }else
    {
        reset();
        movement bestMove;
        vector <movement> valid;
        computeValidMoves(valid); // search all possible validMoves

        // Iterative deepening:
        for (int d = 1; true; d++)
        {
            cout << "depth: " << d << ", current player: " << _current_player << endl;
            // realize the first alpha beta reccurtion to easy place the las best move at first position

            Sint32 alpha = -INF, beta = INF;

            if (valid.empty())
            {
                return; // impossible in our rules because the player isn't invite to play
            }

            if (d > 1)
            {
                auto it = find_if(valid.begin(), valid.end(), [&](const movement &m)
                {
                    return m.ox == bestMove.ox && m.oy == bestMove.oy
                           && m.nx == bestMove.nx && m.ny == bestMove.ny;
                });
                if (it != valid.end())
                    iter_swap(it, valid.begin()); // O(1), pas de copie
            }

            Sint32 bestScore = -INF; // -INFINITY
            for (movement &mv: valid)
            {
                moveInfo info = applyMove(mv);
                Sint32 score = -alphaBetaSeq(d - 1, -beta, -alpha);
                undoMove(info);
#if DEBUG
                //cout << "bestscore : " << bestScore<<", move score: "<<score<< endl;
#endif
                if (score > bestScore)
                {
                    bestScore = score;
                    bestMove = mv;
                }
                if (score > alpha)
                {
                    alpha = score;
                }
                if (alpha >= beta)
                {
                    break;
                }
            }
            _saveBestMove(bestMove);
        }
    }
}

