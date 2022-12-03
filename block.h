
#define MAX_MINERS 200
typedef struct _Block {
    int wallets[MAX_MINERS];
    long int target;
    long int solution;
    int id;
    int is_valid;
    struct _Block *next;
    struct _Block *prev;
} Block;

int addBlock(Block** lista,Block *block);
void blockFree(Block* block);
Block* block_init();
void changeBlock(Block* before, Block*after);
void print_blocks(Block *plast_block, int num_wallets);