#include <veakrnl.h>

static 
LONG 
VEAPI
RtlpGetBalanceFactor(PRTL_BALANCED_NODE Node) {
    if (!Node) return 0;
    return AVL_GET_HEIGHT(Node->Right) - AVL_GET_HEIGHT(Node->Left);
}

static 
PRTL_BALANCED_NODE 
VEAPI
RtlpRotateRight(PRTL_BALANCED_NODE Root) {
    PRTL_BALANCED_NODE NewRoot = Root->Left;
    Root->Left = NewRoot->Right;
    NewRoot->Right = Root;

    // Update tinggi (Balance field) setelah rotasi
    Root->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(Root->Left), AVL_GET_HEIGHT(Root->Right));
    NewRoot->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(NewRoot->Left), AVL_GET_HEIGHT(NewRoot->Right));

    return NewRoot;
}

static 
PRTL_BALANCED_NODE 
VEAPI
RtlpRotateLeft(PRTL_BALANCED_NODE Root) {
    PRTL_BALANCED_NODE NewRoot = Root->Right;
    Root->Right = NewRoot->Left;
    NewRoot->Left = Root;

    // Update tinggi (Balance field) setelah rotasi
    Root->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(Root->Left), AVL_GET_HEIGHT(Root->Right));
    NewRoot->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(NewRoot->Left), AVL_GET_HEIGHT(NewRoot->Right));

    return NewRoot;
}

static 
PRTL_BALANCED_NODE 
VEAPI
RtlpInsertNodeAvl(
    PRTL_BALANCED_NODE Root, 
    PRTL_BALANCED_NODE NewNode, 
    PRTL_AVL_COMPARE_ROUTINE Compare) 
{
    // 1. Fase Insert BST (Binary Search Tree) Normal
    if (!Root) return NewNode;

    LONG Result = Compare(NewNode, Root);

    if (Result < 0) {
        Root->Left = RtlpInsertNodeAvl(Root->Left, NewNode, Compare);
    } else if (Result > 0) {
        Root->Right = RtlpInsertNodeAvl(Root->Right, NewNode, Compare);
    } else {
        // Data duplikat tidak diizinkan di VMM
        return Root; 
    }

    // 2. Update tinggi node saat ini
    Root->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(Root->Left), AVL_GET_HEIGHT(Root->Right));

    // 3. Dapatkan Balance Factor untuk mengecek keseimbangan
    LONG BalanceFactor = RtlpGetBalanceFactor(Root);

    // 4. Fase Re-balancing (4 Kasus AVL)

    // Kasus Left-Left
    if (BalanceFactor < -1 && Compare(NewNode, Root->Left) < 0)
        return RtlpRotateRight(Root);

    // Kasus Right-Right
    if (BalanceFactor > 1 && Compare(NewNode, Root->Right) > 0)
        return RtlpRotateLeft(Root);

    // Kasus Left-Right
    if (BalanceFactor < -1 && Compare(NewNode, Root->Left) > 0) {
        Root->Left = RtlpRotateLeft(Root->Left);
        return RtlpRotateRight(Root);
    }

    // Kasus Right-Left
    if (BalanceFactor > 1 && Compare(NewNode, Root->Right) < 0) {
        Root->Right = RtlpRotateRight(Root->Right);
        return RtlpRotateLeft(Root);
    }

    return Root;
}

VOID 
VEAPI 
RtlInsertElementAvl(PRTL_AVL_TREE Tree, PRTL_BALANCED_NODE NewNode) 
{
    NewNode->Left = NULL;
    NewNode->Right = NULL;
    NewNode->Balance = 1; // Node baru selalu punya tinggi 1
    Tree->Root = RtlpInsertNodeAvl(Tree->Root, NewNode, Tree->CompareRoutine);
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

static 
PRTL_BALANCED_NODE 
VEAPI
RtlpDeleteNodeAvl(
    PRTL_BALANCED_NODE Root, 
    PRTL_BALANCED_NODE TargetNode, 
    PRTL_AVL_COMPARE_ROUTINE Compare) 
{
    if (!Root) return NULL;

    LONG Result = Compare(TargetNode, Root);

    // 1. Fase Pencarian Node (Standar BST)
    if (Result < 0) {
        Root->Left = RtlpDeleteNodeAvl(Root->Left, TargetNode, Compare);
    } else if (Result > 0) {
        Root->Right = RtlpDeleteNodeAvl(Root->Right, TargetNode, Compare);
    } else {
        // NODE DITEMUKAN! Mari kita eksekusi.
        
        // Kasus 1 & 2: Punya 0 atau 1 anak
        if (!Root->Left || !Root->Right) {
            PRTL_BALANCED_NODE Temp = Root->Left ? Root->Left : Root->Right;
            return Temp; // Langsung gantikan posisi Root dengan anaknya (atau NULL)
        } 
        // Kasus 3: Punya 2 anak
        else {
            // Cari suksesor (nilai terkecil di cabang kanan)
            PRTL_BALANCED_NODE Successor = RtlpFindMinNode(Root->Right);
            
            // Cabut suksesor dari posisinya yang lama di cabang kanan
            Root->Right = RtlpDeleteNodeAvl(Root->Right, Successor, Compare);
            
            // Suksesor mengambil alih takhta (koneksi) milik Root lama
            Successor->Left = Root->Left;
            Successor->Right = Root->Right;
            
            Root = Successor; // Root baru di posisi ini adalah si suksesor
        }
    }

    if (!Root) return NULL;

    // 2. Update tinggi node yang baru
    Root->Balance = 1 + AVL_MAX(AVL_GET_HEIGHT(Root->Left), AVL_GET_HEIGHT(Root->Right));

    // 3. Cek Keseimbangan
    LONG BalanceFactor = RtlpGetBalanceFactor(Root);

    // 4. Fase Re-balancing (4 Kasus AVL Delete)
    if (BalanceFactor < -1 && RtlpGetBalanceFactor(Root->Left) <= 0)
        return RtlpRotateRight(Root);

    if (BalanceFactor < -1 && RtlpGetBalanceFactor(Root->Left) > 0) {
        Root->Left = RtlpRotateLeft(Root->Left);
        return RtlpRotateRight(Root);
    }

    if (BalanceFactor > 1 && RtlpGetBalanceFactor(Root->Right) >= 0)
        return RtlpRotateLeft(Root);

    if (BalanceFactor > 1 && RtlpGetBalanceFactor(Root->Right) < 0) {
        Root->Right = RtlpRotateRight(Root->Right);
        return RtlpRotateLeft(Root);
    }

    return Root;
}

VOID 
VEAPI 
RtlDeleteElementAvl(PRTL_AVL_TREE Tree, PRTL_BALANCED_NODE TargetNode) 
{
    if (Tree->Root && TargetNode) {
        Tree->Root = RtlpDeleteNodeAvl(Tree->Root, TargetNode, Tree->CompareRoutine);
    }
}
