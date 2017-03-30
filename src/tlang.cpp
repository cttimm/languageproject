
// -------------------------------------- //
// Charles Timmerman - cttimm4427@ung.edu //
// -------------------------------------- //

#include "parser.h"

static void MainLoop() {
    while(1) {
        fprintf(stderr, "tlang > ");
        switch (currToken) {
            case _EOF:
                return;
            case ';':
                handle_return();
                break;
            case _FN:
                handle_definition();
                break;
            case _IMPORT:
                handle_import();
                break;
            case _EXIT:
                fprintf(stderr, "exiting...\n");
                return;
            default:
                handle_top();
                break;
        }
    }
}


int main() {
    binopPrec['<'] = 10;
    binopPrec['+'] = 20;
    binopPrec['-'] = 20;
    binopPrec['*'] = 40;

    fprintf(stderr, "tlang > ");
    get_next_token();

    // Memory allocation
    MODULE = llvm::make_unique<llvm::Module>("tlang JIT", CONTEXT);
    MainLoop();
    // Dumps all messages upon closing with CTRL-D
    MODULE->print(llvm::errs(), nullptr);

    return 0;
};