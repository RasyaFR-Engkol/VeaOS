#include <rtl.h>

static
VOID
RtlRbLeftRotate(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE X)
{
    PRTL_BALANCED_NODE Y = X->Right;
    X->Right = Y->Left;

    if (Y->Left != NULL) {
        Y->Left->Parent = X;
    }

    Y->Parent = X->Parent;

    if (X->Parent == NULL) {
        Tree->Root = Y;
    } else if (X == X->Parent->Left) {
        X->Parent->Left = Y;
    } else {
        X->Parent->Right = Y;
    }

    Y->Left = X;
    X->Parent = Y;
}

static 
VOID
RtlRbRightRotate(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE Y)
{
    PRTL_BALANCED_NODE X = Y->Left;
    Y->Left = X->Right;

    if (X->Right != NULL) {
        X->Right->Parent = Y;
    }

    X->Parent = Y->Parent;

    if (Y->Parent == NULL) {
        Tree->Root = X;
    } else if (Y == Y->Parent->Right) {
        Y->Parent->Right = X;
    } else {
        Y->Parent->Left = X;
    }

    X->Right = Y;
    Y->Parent = X;
}

static
VOID
RtlRbInsertFixup(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE Z) 
{
    while (Z->Parent != NULL && RTL_RB_COLOR(Z->Parent) == RTL_RB_RED) 
    {
        if (Z->Parent == Z->Parent->Parent->Left) 
        {
            PRTL_BALANCED_NODE Y = Z->Parent->Parent->Right; // Paman Z
            
            if (Y != NULL && RTL_RB_COLOR(Y) == RTL_RB_RED) 
            {
                RTL_RB_SET_COLOR(Z->Parent, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(Y, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(Z->Parent->Parent, RTL_RB_RED);
                Z = Z->Parent->Parent;
            } 
            else 
            {
                if (Z == Z->Parent->Right) 
                {
                    Z = Z->Parent;
                    RtlRbLeftRotate(Tree, Z);
                }
                RTL_RB_SET_COLOR(Z->Parent, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(Z->Parent->Parent, RTL_RB_RED);
                RtlRbRightRotate(Tree, Z->Parent->Parent);
            }
        } else {
            PRTL_BALANCED_NODE Y = Z->Parent->Parent->Left; // Paman Z
            
            if (Y != NULL && RTL_RB_COLOR(Y) == RTL_RB_RED) {
                RTL_RB_SET_COLOR(Z->Parent, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(Y, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(Z->Parent->Parent, RTL_RB_RED);
                Z = Z->Parent->Parent;
            } else {
                if (Z == Z->Parent->Left) {
                    Z = Z->Parent;
                    RtlRbRightRotate(Tree, Z);
                }
                RTL_RB_SET_COLOR(Z->Parent, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(Z->Parent->Parent, RTL_RB_RED);
                RtlRbLeftRotate(Tree, Z->Parent->Parent);
            }
        }
    }
    RTL_RB_SET_COLOR(Tree->Root, RTL_RB_BLACK);
}

static VOID RtlRbTransplant(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE U, PRTL_BALANCED_NODE V) {
    if (U->Parent == NULL) {
        Tree->Root = V;
    } else if (U == U->Parent->Left) {
        U->Parent->Left = V;
    } else {
        U->Parent->Right = V;
    }
    
    if (V != NULL) {
        V->Parent = U->Parent;
    }
}

static VOID RtlRbDeleteFixup(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE X, PRTL_BALANCED_NODE XParent) {
    while (X != Tree->Root && (X == NULL || RTL_RB_COLOR(X) == RTL_RB_BLACK)) {
        if (X == XParent->Left) {
            PRTL_BALANCED_NODE W = XParent->Right; // Saudara X
            if (RTL_RB_COLOR(W) == RTL_RB_RED) {
                RTL_RB_SET_COLOR(W, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(XParent, RTL_RB_RED);
                RtlRbLeftRotate(Tree, XParent);
                W = XParent->Right;
            }
            if ((W->Left == NULL || RTL_RB_COLOR(W->Left) == RTL_RB_BLACK) &&
                (W->Right == NULL || RTL_RB_COLOR(W->Right) == RTL_RB_BLACK)) {
                RTL_RB_SET_COLOR(W, RTL_RB_RED);
                X = XParent;
                XParent = X->Parent;
            } else {
                if (W->Right == NULL || RTL_RB_COLOR(W->Right) == RTL_RB_BLACK) {
                    if (W->Left != NULL) RTL_RB_SET_COLOR(W->Left, RTL_RB_BLACK);
                    RTL_RB_SET_COLOR(W, RTL_RB_RED);
                    RtlRbRightRotate(Tree, W);
                    W = XParent->Right;
                }
                RTL_RB_SET_COLOR(W, RTL_RB_COLOR(XParent));
                RTL_RB_SET_COLOR(XParent, RTL_RB_BLACK);
                if (W->Right != NULL) RTL_RB_SET_COLOR(W->Right, RTL_RB_BLACK);
                RtlRbLeftRotate(Tree, XParent);
                X = Tree->Root;
            }
        } else {
            PRTL_BALANCED_NODE W = XParent->Left;
            if (RTL_RB_COLOR(W) == RTL_RB_RED) {
                RTL_RB_SET_COLOR(W, RTL_RB_BLACK);
                RTL_RB_SET_COLOR(XParent, RTL_RB_RED);
                RtlRbRightRotate(Tree, XParent);
                W = XParent->Left;
            }
            if ((W->Right == NULL || RTL_RB_COLOR(W->Right) == RTL_RB_BLACK) &&
                (W->Left == NULL || RTL_RB_COLOR(W->Left) == RTL_RB_BLACK)) {
                RTL_RB_SET_COLOR(W, RTL_RB_RED);
                X = XParent;
                XParent = X->Parent;
            } else {
                if (W->Left == NULL || RTL_RB_COLOR(W->Left) == RTL_RB_BLACK) {
                    if (W->Right != NULL) RTL_RB_SET_COLOR(W->Right, RTL_RB_BLACK);
                    RTL_RB_SET_COLOR(W, RTL_RB_RED);
                    RtlRbLeftRotate(Tree, W);
                    W = XParent->Left;
                }
                RTL_RB_SET_COLOR(W, RTL_RB_COLOR(XParent));
                RTL_RB_SET_COLOR(XParent, RTL_RB_BLACK);
                if (W->Left != NULL) RTL_RB_SET_COLOR(W->Left, RTL_RB_BLACK);
                RtlRbRightRotate(Tree, XParent);
                X = Tree->Root;
            }
        }
    }
    if (X != NULL) {
        RTL_RB_SET_COLOR(X, RTL_RB_BLACK);
    }
}

VOID VEAPI RtlInsertElementRb(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE NewNode) {
    PRTL_BALANCED_NODE Y = NULL;
    PRTL_BALANCED_NODE X = Tree->Root;
    BOOLEAN Leftmost = TRUE; // Lacak posisi node untuk cache CFS

    NewNode->Left = NULL;
    NewNode->Right = NULL;
    RTL_RB_SET_COLOR(NewNode, RTL_RB_RED);

    while (X != NULL) {
        Y = X;
        if (Tree->CompareRoutine(NewNode, X) < 0) {
            X = X->Left;
        } else {
            X = X->Right;
            Leftmost = FALSE; // Bukan jalur leftmost lagi
        }
    }

    NewNode->Parent = Y;
    if (Y == NULL) {
        Tree->Root = NewNode;
        Tree->Leftmost = NewNode; // Root adalah leftmost pertama
    } else if (Tree->CompareRoutine(NewNode, Y) < 0) {
        Y->Left = NewNode;
        if (Leftmost) {
            Tree->Leftmost = NewNode; // Update cache
        }
    } else {
        Y->Right = NewNode;
    }

    RtlRbInsertFixup(Tree, NewNode);
}

VOID VEAPI RtlDeleteElementRb(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE Z) {
    PRTL_BALANCED_NODE Y = Z;
    PRTL_BALANCED_NODE X = NULL;
    PRTL_BALANCED_NODE XParent = NULL;
    LONG YOriginalColor = RTL_RB_COLOR(Y);

    // CFS Cache Fix: Update Leftmost jika kita menghapus node leftmost saat ini
    if (Tree->Leftmost == Z) {
        PRTL_BALANCED_NODE Next = Z->Right;
        if (Next != NULL) {
            while (Next->Left != NULL) Next = Next->Left;
            Tree->Leftmost = Next;
        } else {
            Tree->Leftmost = Z->Parent;
        }
    }

    if (Z->Left == NULL) {
        X = Z->Right;
        XParent = Z->Parent;
        RtlRbTransplant(Tree, Z, Z->Right);
    } else if (Z->Right == NULL) {
        X = Z->Left;
        XParent = Z->Parent;
        RtlRbTransplant(Tree, Z, Z->Left);
    } else {
        Y = Z->Right;
        while (Y->Left != NULL) {
            Y = Y->Left;
        }
        YOriginalColor = RTL_RB_COLOR(Y);
        X = Y->Right;
        
        if (Y->Parent == Z) {
            XParent = Y;
        } else {
            XParent = Y->Parent;
            RtlRbTransplant(Tree, Y, Y->Right);
            Y->Right = Z->Right;
            Y->Right->Parent = Y;
        }
        RtlRbTransplant(Tree, Z, Y);
        Y->Left = Z->Left;
        Y->Left->Parent = Y;
        RTL_RB_SET_COLOR(Y, RTL_RB_COLOR(Z));
    }

    if (YOriginalColor == RTL_RB_BLACK) {
        RtlRbDeleteFixup(Tree, X, XParent);
    }

    Z->Left = NULL;
    Z->Right = NULL;
    Z->Parent = NULL;
}

VOID VEAPI RtlInitializeRbTree(PRTL_RB_TREE Tree, PRTL_RB_COMPARE_ROUTINE CompareRoutine) {
    Tree->Root = NULL;
    Tree->Leftmost = NULL; // Sangat penting untuk O(1) sched_pick_next
    Tree->CompareRoutine = CompareRoutine;
}

PRTL_BALANCED_NODE VEAPI RtlLookupElementRb(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE SearchNode) {
    PRTL_BALANCED_NODE Current = Tree->Root;

    // Traversal biasa O(log N) berdasarkan hasil CompareRoutine
    while (Current != NULL) {
        LONG Result = Tree->CompareRoutine(SearchNode, Current);
        
        if (Result == 0) {
            return Current; // Node ditemukan
        } else if (Result < 0) {
            Current = Current->Left;
        } else {
            Current = Current->Right;
        }
    }

    return NULL; // Tidak ditemukan
}

PRTL_BALANCED_NODE VEAPI RtlGetNextNodeRb(PRTL_BALANCED_NODE Node) {
    if (Node == NULL) {
        return NULL;
    }

    // Kasus 1: Jika memiliki child di kanan, suksesor adalah node paling kiri di subtree kanan.
    if (Node->Right != NULL) {
        Node = Node->Right;
        while (Node->Left != NULL) {
            Node = Node->Left;
        }
        return Node;
    }

    // Kasus 2: Jika tidak memiliki child di kanan, suksesor ada di ancestor (parent).
    // Terus naik ke atas selama node saat ini adalah child KANAN dari parent-nya.
    PRTL_BALANCED_NODE Parent = Node->Parent;
    while (Parent != NULL && Node == Parent->Right) {
        Node = Parent;
        Parent = Parent->Parent;
    }

    // Jika Parent menjadi NULL, berarti node awal adalah node paling kanan di tree (tidak ada suksesor).
    return Parent;
}