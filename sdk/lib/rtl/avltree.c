#include <rtl.h>

static 
LONG 
VEAPI
RtlpGetBalanceFactor(PRTL_BALANCED_NODE Node) {
    if (!Node) return 0;
    return AVL_GET_HEIGHT(Node->Right) - AVL_GET_HEIGHT(Node->Left);
}

static 
VOID
VEAPI
RtlpRotateRightIterative(PRTL_AVL_TREE Tree, PRTL_BALANCED_NODE Node) {
    PRTL_BALANCED_NODE LeftChild = Node->Left;
    
    // Geser anak kanan dari LeftChild menjadi anak kiri Node
    Node->Left = LeftChild->Right;
    if (LeftChild->Right) {
        LeftChild->Right->Parent = Node;
    }
    
    // Hubungkan LeftChild ke Parent dari Node
    LeftChild->Parent = Node->Parent;
    if (!Node->Parent) {
        Tree->Root = LeftChild; // Node sebelumnya adalah Root
    } else if (Node == Node->Parent->Right) {
        Node->Parent->Right = LeftChild;
    } else {
        Node->Parent->Left = LeftChild;
    }
    
    // Selesaikan rotasi
    LeftChild->Right = Node;
    Node->Parent = LeftChild;

    // Update tinggi (Balance) dari bawah ke atas
    Node->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(Node->Left), AVL_GET_HEIGHT(Node->Right));
    LeftChild->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(LeftChild->Left), AVL_GET_HEIGHT(LeftChild->Right));
}

static
VOID
VEAPI
RtlpRotateLeftIterative(
    PRTL_AVL_TREE Tree,
    PRTL_BALANCED_NODE Node
)
{
    PRTL_BALANCED_NODE RightChild = Node->Right;

    Node->Right = RightChild->Left;
    if(RightChild->Left)
    {
        RightChild->Left->Parent = Node;
    }

    RightChild->Parent = Node->Parent;
    if (!Node->Parent) {
        Tree->Root = RightChild;
    } else if (Node == Node->Parent->Left) {
        Node->Parent->Left = RightChild;
    } else {
        Node->Parent->Right = RightChild;
    }
    
    RightChild->Left = Node;
    Node->Parent = RightChild;

    Node->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(Node->Left), AVL_GET_HEIGHT(Node->Right));
    RightChild->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(RightChild->Left), AVL_GET_HEIGHT(RightChild->Right));
}

VOID 
VEAPI 
RtlInsertElementAvl(PRTL_AVL_TREE Tree, PRTL_BALANCED_NODE NewNode) 
{
    NewNode->Left = NULL;
    NewNode->Right = NULL;
    NewNode->Parent = NULL;
    NewNode->Balance = 1; // Tinggi awal node baru

    if (!Tree->Root) {
        Tree->Root = NewNode;
        Tree->Leftmost = NewNode;
        return;
    }

    PRTL_BALANCED_NODE Current = Tree->Root;
    PRTL_BALANCED_NODE Parent = NULL;
    LONG Result;

    while(Current != NULL)
    {
        Parent = Current;
        Result = Tree->CompareRoutine(NewNode, Current);

        if (Result < 0) {
            Current = Current->Left;
        } else {
            // FIX DUPLIKAT: Jika Result == 0 (sama) atau Result > 0, lempar ke Kanan.
            // Ini mencegah data hilang dan memastikan FIFO (First In First Out)
            // untuk data identik dalam operasi penelusuran.
            Current = Current->Right;
        }
    }

    NewNode->Parent = Parent;
    if (Result < 0) {
        Parent->Left = NewNode;
    } else {
        Parent->Right = NewNode;
    }

    if (!Tree->Leftmost || Tree->CompareRoutine(NewNode, Tree->Leftmost) < 0) {
        Tree->Leftmost = NewNode;
    }

    // ==========================================
    // FASE 2: NAIK (Re-balancing iteratif)
    // ==========================================
    Current = Parent; // Mulai dari parent node yang baru diinsert
    while (Current != NULL) {
        // Update tinggi node saat ini
        Current->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(Current->Left), AVL_GET_HEIGHT(Current->Right));

        // Cek selisih tinggi (Kanan - Kiri)
        LONG BalanceFactor = AVL_GET_HEIGHT(Current->Right) - AVL_GET_HEIGHT(Current->Left);

        // Kasus Left-Heavy
        if (BalanceFactor < -1) {
            LONG LeftBF = AVL_GET_HEIGHT(Current->Left->Right) - AVL_GET_HEIGHT(Current->Left->Left);
            if (LeftBF > 0) { // Kasus Left-Right
                RtlpRotateLeftIterative(Tree, Current->Left);
            }
            RtlpRotateRightIterative(Tree, Current); // Kasus Left-Left
        }
        // Kasus Right-Heavy
        else if (BalanceFactor > 1) {
            LONG RightBF = AVL_GET_HEIGHT(Current->Right->Right) - AVL_GET_HEIGHT(Current->Right->Left);
            if (RightBF < 0) { // Kasus Right-Left
                RtlpRotateRightIterative(Tree, Current->Right);
            }
            RtlpRotateLeftIterative(Tree, Current); // Kasus Right-Right
        }

        // Naik ke level berikutnya
        Current = Current->Parent;
    }
}

static 
PRTL_BALANCED_NODE 
VEAPI
RtlpFindMinNode(PRTL_BALANCED_NODE Node) 
{
    while (Node && Node->Left) {
        Node = Node->Left;
    }
    return Node;
}

VOID 
VEAPI 
RtlDeleteElementAvl(PRTL_AVL_TREE Tree, PRTL_BALANCED_NODE TargetNode) 
{
    if (!Tree->Root || !TargetNode) return;

    if (Tree->Leftmost == TargetNode) {
        if (TargetNode->Right) {
            // Jika ada cabang kanan, Leftmost baru adalah nilai terkecil di cabang kanan
            PRTL_BALANCED_NODE NextMin = TargetNode->Right;
            while (NextMin->Left) {
                NextMin = NextMin->Left;
            }
            Tree->Leftmost = NextMin;
        } else {
            // Jika tidak ada cabang kanan, Leftmost baru adalah Parent-nya
            Tree->Leftmost = TargetNode->Parent;
        }
    }

    PRTL_BALANCED_NODE NodeToBalance = NULL;
    PRTL_BALANCED_NODE ParentNode = TargetNode->Parent;

    if (!TargetNode->Left || !TargetNode->Right) {
        PRTL_BALANCED_NODE Child = TargetNode->Left ? TargetNode->Left : TargetNode->Right;
        
        if (Child) Child->Parent = ParentNode;

        if (!ParentNode) {
            Tree->Root = Child;
        } else if (TargetNode == ParentNode->Left) {
            ParentNode->Left = Child;
        } else {
            ParentNode->Right = Child;
        }
        
        NodeToBalance = ParentNode;
    }
    else {
        // Cari suksesor (node terkecil di sub-pohon kanan)
        PRTL_BALANCED_NODE Successor = TargetNode->Right;
        while (Successor->Left) {
            Successor = Successor->Left;
        }

        PRTL_BALANCED_NODE SuccessorParent = Successor->Parent;
        PRTL_BALANCED_NODE SuccessorRight = Successor->Right;

        // Jika suksesor BUKAN anak kanan langsung dari TargetNode
        if (SuccessorParent != TargetNode) {
            SuccessorParent->Left = SuccessorRight;
            if (SuccessorRight) SuccessorRight->Parent = SuccessorParent;
            
            Successor->Right = TargetNode->Right;
            TargetNode->Right->Parent = Successor;
            
            NodeToBalance = SuccessorParent;
        } else {
            NodeToBalance = Successor;
        }

        // Posisikan suksesor di tempat TargetNode
        Successor->Parent = ParentNode;
        Successor->Left = TargetNode->Left;
        TargetNode->Left->Parent = Successor;

        if (!ParentNode) {
            Tree->Root = Successor;
        } else if (TargetNode == ParentNode->Left) {
            ParentNode->Left = Successor;
        } else {
            ParentNode->Right = Successor;
        }
    }
    while (NodeToBalance != NULL) {
        // Update tinggi
        NodeToBalance->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(NodeToBalance->Left), AVL_GET_HEIGHT(NodeToBalance->Right));
        
        // Hitung selisih
        LONG BalanceFactor = AVL_GET_HEIGHT(NodeToBalance->Right) - AVL_GET_HEIGHT(NodeToBalance->Left);

        if (BalanceFactor < -1) {
            LONG LeftBF = AVL_GET_HEIGHT(NodeToBalance->Left->Right) - AVL_GET_HEIGHT(NodeToBalance->Left->Left);
            if (LeftBF > 0) RtlpRotateLeftIterative(Tree, NodeToBalance->Left);
            RtlpRotateRightIterative(Tree, NodeToBalance);
        }
        else if (BalanceFactor > 1) {
            LONG RightBF = AVL_GET_HEIGHT(NodeToBalance->Right->Right) - AVL_GET_HEIGHT(NodeToBalance->Right->Left);
            if (RightBF < 0) RtlpRotateRightIterative(Tree, NodeToBalance->Right);
            RtlpRotateLeftIterative(Tree, NodeToBalance);
        }

        NodeToBalance = NodeToBalance->Parent;
    }
}

PRTL_BALANCED_NODE
VEAPI
RtlLookupElementAvl(
    PRTL_AVL_TREE Tree, 
    PRTL_BALANCED_NODE TargetNode
)
{
    PRTL_BALANCED_NODE Current = Tree->Root;
    
    while (Current != NULL) {
        LONG Result = Tree->CompareRoutine(TargetNode, Current);
        
        if (Result < 0) {
            Current = Current->Left;
        } else if (Result > 0) {
            Current = Current->Right;
        } else {
            // Node ditemukan
            return Current; 
        }
    }

    return NULL;
}

PRTL_BALANCED_NODE 
VEAPI 
RtlGetNextNodeAvl(PRTL_BALANCED_NODE Node) 
{
    if (!Node) return NULL;

    // Jika punya anak kanan, suksesor adalah node paling kiri di cabang kanan
    if (Node->Right) {
        Node = Node->Right;
        while (Node->Left) Node = Node->Left;
        return Node;
    }

    // Jika tidak punya anak kanan, naik ke Parent
    // Selama node saat ini adalah anak kanan dari Parent-nya, terus naik
    PRTL_BALANCED_NODE Parent = Node->Parent;
    while (Parent && Node == Parent->Right) {
        Node = Parent;
        Parent = Parent->Parent;
    }

    return Parent;
}
