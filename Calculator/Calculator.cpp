#include <iostream>
#include <string>
using namespace std;

struct Tree {
    char inf;
    Tree* left = nullptr;
    Tree* right = nullptr;
};

static string ExprStr;
static char scanSymbol;
static int SI;
static string DIGIT = "0123456789";
Tree* root;


static void InsNode(Tree*& tree, char c) {
    tree = new Tree();
    tree->inf = c;
}

static void WriteExpr(Tree* tree) {
    if (tree != nullptr) {
        WriteExpr(tree->left);
        WriteExpr(tree->right);
        cout << tree->inf << endl;
    }
}

static void NextSymbol() {
    if (SI < ExprStr.size() - 1) {
        SI++;
        scanSymbol = ExprStr[SI];
    }
    else
        SI = -1;
}

static bool Contain(string syms, char sym) {
    for (int i = 0; i < syms.size(); i++) {
        if (sym == syms[i])
            return true;
    }
    return false;
}

static void ADDEND(Tree*& tree);

static void EXPRESSION(Tree*& tree) {
    Tree* O = nullptr;
    tree = nullptr;
    if (Contain(DIGIT, scanSymbol) || scanSymbol == '(') {
        ADDEND(O);
        while (((scanSymbol == '+') || (scanSymbol == '-')) && (SI != -1)) {
            InsNode(tree, scanSymbol);
            tree->left = O;
            NextSymbol();
            if (SI != -1) {
                ADDEND(tree->right);
            }
            O = tree;
        }
        if (tree == nullptr)
            tree = O;

    }
    else {
        cout << "Wrong beginning\n";
    }
}

static void ADDEND(Tree*& tree) {
    if (Contain(DIGIT, scanSymbol)) {
        InsNode(tree, scanSymbol);
        NextSymbol();
    }
    else {
        if (scanSymbol == '(') {
            NextSymbol();
            if (SI != -1)
                EXPRESSION(tree);
            if (SI != -1 && scanSymbol == ')')
                NextSymbol();
            else
                cout << "There is no last brake\n";
        }
        else {
            cout << "Wrong beginning\n";
        }
    }
}

static int Calculate(Tree*& tree) {
    if (Contain(DIGIT, tree->inf))
        return (int)tree->inf - 48;
    else {
        if (tree->inf == '+')
            return Calculate(tree->left) + Calculate(tree->right);
        else
            return Calculate(tree->left) - Calculate(tree->right);
    }

}


int main()
{
    root = nullptr;
    cout << "Write an expression:\n";
    cin >> ExprStr;
    SI = 0;
    scanSymbol = ExprStr[SI];
    EXPRESSION(root);
    if (SI == -1){
        cout << "Correct\n";
    }
    else {
        cout << "Too much symbols\n";
    }
    if (root != nullptr) {
        cout << "tree:\n";
        WriteExpr(root);
        cout << endl << "The result = " << Calculate(root) << endl;
    }
}
