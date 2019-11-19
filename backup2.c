#include <stdio.h>
#include <stdlib.h>

typedef struct Node
{
    int data;
    struct Node *rlink;
    struct Node *llink;
}Node;

//Following is the structure for Insert operation
struct Insert
{
	//Following is the structure for arguments passed to Insert
	struct Arguments
	{
		Node *p;//node to be inserted
		Node *x;//p is inserted next to x
	}args;
	struct LocalVariables
	{
		Node *x_rlink_llink;//This announces the value of x->rlink->llink which needs to be set to p.
		Node **x_rlink_llink_address;//This announces the address of x->rlink->llink which is used as destination in InterlockedCompareExchange.
		Node *x_rlink;//This announces the value of x->rlink which needs to be set to p.
		Node **x_rlink_address;//This announces the address of x->rlink which is used as destination in InterlockedCompareExchange.
	}lv;
};

//Following is the structure for Delete operation
struct Delete
{
	//Following is the structure for arguments passed to Delete
	struct _Arguments
	{
		Node *x;//node to be deleted.
	}args;
	struct _LocalVariables
	{
		//Following variables announce the values and addresses of pointers found in nodes around x (including x as well).
		Node *x_llink;
		Node **x_llink_address;
		Node *x_rlink;
		Node **x_rlink_address;
		Node *x_llink_rlink;
		Node **x_llink_rlink_address;
		Node *x_rlink_llink;
		Node **x_rlink_llink_address;
		Node *x_llink_llink;
		Node **x_llink_llink_address;
		Node *x_rlink_rlink;
		Node **x_rlink_rlink_address;
		Node *x_llink_llink_rlink;
		Node **x_llink_llink_rlink_address;
		Node *x_rlink_rlink_llink;
		Node **x_rlink_rlink_llink_address;

		Node *replacement_x_llink;//This points to the replacement for the node left to x.
		Node *replacement_x_rlink;//This points to the replacement for the node right to x.
	}lv;
};

enum OperationName{NONE=0,INSERT=1,DELET=2};
//Following structure contains the operations defined earlier.
typedef struct AnnounceOp
{
	enum OperationName opName;
	union
	{
		struct Insert insert;
		struct Delete del;
	};
}AnnounceOp;

typedef struct LinkedList
{
	volatile AnnounceOp* announce;//current announcement
	Node *first;//first node
	Node *end;//end node
}LinkedList;

LinkedList l;

void initialize()
{
	//current announcement is that no operation is in progress
	l.announce=(AnnounceOp*)_aligned_malloc(sizeof(struct AnnounceOp),InterLockedAlign );//new AnnounceOp;
	assert(l.announce);
	l.announce->opName=NONE;

	//create 4 node doubly linked list
	l.first=(Node *)_aligned_malloc(sizeof(struct Node),InterLockedAlign );//new Node;
	assert(l.first);
	l.end=(Node *)_aligned_malloc(sizeof(struct Node),InterLockedAlign );
	assert(l.end);
	l.first->rlink=(Node *)_aligned_malloc(sizeof(struct Node),InterLockedAlign );
	assert(l.first->rlink);
	l.first->rlink->rlink=(Node *)_aligned_malloc(sizeof(struct Node),InterLockedAlign );
	assert(l.first->rlink->rlink);
	l.first->llink=0;
	l.first->rlink->llink=l.first;
	l.first->rlink->rlink->rlink=l.end;
	l.first->rlink->rlink->llink=l.first->rlink;
	l.end->rlink=0;
	l.end->llink=l.first->rlink->rlink;
}

//insert node p to the right of node x
int Insert(Node *p,Node *x)
{
	if(p==0||x==0) return 0;
	AnnounceOp *curAnnouncedOp;
	AnnounceOp *nextAnnounceOp=(AnnounceOp*)_aligned_malloc(sizeof(struct AnnounceOp),InterLockedAlign );//To announce an insert operation.
	assert(nextAnnounceOp);
	while(1)
	{
		curAnnouncedOp=(AnnounceOp *)l.announce;
		AnnounceOp *hp0=curAnnouncedOp;
		if(curAnnouncedOp!=l.announce) continue;
		if(curAnnouncedOp->opName==NONE)
		{
			try
			{
				if(l.first==x||l.end==x||l.end->llink==x)//insertion should not be after first or after end or after one node before end
				{
					_aligned_free(nextAnnounceOp);
					return 0;
				}
				p->llink=x;//set p's left as x
				p->rlink=x->rlink;//set p's right as x's right
				if(p->rlink==0||p->llink==0)  goto label;
				nextAnnounceOp->opName=INSERT;//set INSERT as the next operation
				nextAnnounceOp->insert.args.p=p;
				nextAnnounceOp->insert.args.x=x;

				//announce the value of x->rlink which needs to be set to p
				nextAnnounceOp->insert.lv.x_rlink=x->rlink;
				if(nextAnnounceOp->insert.lv.x_rlink==0)  goto label;//node x is no more in the linked list
				Node *hp2=nextAnnounceOp->insert.lv.x_rlink;//set hazard pointer
				if(x->rlink!=nextAnnounceOp->insert.lv.x_rlink)  goto label;//check that hazard pointer has been set accurately
				nextAnnounceOp->insert.lv.x_rlink_address=&x->rlink;//announce the address of x->rlink to be used as destination in InterlockedCompareExchange

				//announce the value of x->rlink->llink which needs to be set to p
				nextAnnounceOp->insert.lv.x_rlink_llink=nextAnnounceOp->insert.lv.x_rlink->llink;
				if(nextAnnounceOp->insert.lv.x_rlink_llink==0)  goto label;//node next to node x is unlinked
				Node *hp1=nextAnnounceOp->insert.lv.x_rlink_llink;//set hazard pointer
				if(nextAnnounceOp->insert.lv.x_rlink->llink!=nextAnnounceOp->insert.lv.x_rlink_llink)  goto label;//check hazard pointer is set correctly
				nextAnnounceOp->insert.lv.x_rlink_llink_address=&nextAnnounceOp->insert.lv.x_rlink->llink;//announce the address of x->rlink->llink to be used as destination in InterlockedCompareExchange.



				//Check that announced addresses has not changed
				/*if(&x->rlink->llink!=nextAnnounceOp->insert.lv.x_rlink_llink_address)  goto label;
				if(&x->rlink!=nextAnnounceOp->insert.lv.x_rlink_address)  goto label;*/

				//To announce the start of insert operation.
				void *v1=reinterpret_cast<void>(nextAnnounceOp);
				void *v2=reinterpret_cast<void>(curAnnouncedOp);
				void *res=(void *)InterlockedCompareExchange(reinterpret_cast<volatile>(&l.announce),(LONG)v1,(LONG)v2);
				if(res==v2)
				{
					//RetireNode(curAnnouncedOp);
					InsertHelper(nextAnnounceOp);
					return 1;
				}
			}
			catch(...)
			{
				printf("Exception in Insert\n");
				_aligned_free(nextAnnounceOp);
				return 0;
			}
		}
		else if(curAnnouncedOp->opName==INSERT)
		{
			InsertHelper(curAnnouncedOp);
		}
		else if(curAnnouncedOp->opName==DELET)
		{
			DeleteHelper(curAnnouncedOp);
		}
	}
label:
	_aligned_free(nextAnnounceOp);
	return 0;
}

int Delete(Node *x)
{
	if(x==0) return 0;
	AnnounceOp *curAnnouncedOp;
	AnnounceOp *nextAnnounceOp=(AnnounceOp*)_aligned_malloc(sizeof(struct AnnounceOp),InterLockedAlign );//new AnnounceOp;//To announce a delete operation.
	assert(nextAnnounceOp);
	Node *replacement_x_llink=(Node *)_aligned_malloc(sizeof(struct Node),InterLockedAlign );//new Node;
	assert(replacement_x_llink);
	Node *replacement_x_rlink=(Node *)_aligned_malloc(sizeof(struct Node),InterLockedAlign );//new Node;
	assert(replacement_x_rlink);
	while(1)
	{
		curAnnouncedOp=(AnnounceOp *)l.announce;
		AnnounceOp *hp0=curAnnouncedOp;
		if(curAnnouncedOp!=l.announce) continue;
		if(curAnnouncedOp->opName==NONE)
		{
			try
			{
				if(l.first==x||l.end==x||l.first->rlink==x||l.end->llink==x) //check x is not one of the four dummy nodes
				{
					_aligned_free(nextAnnounceOp);
					_aligned_free(replacement_x_llink);
					_aligned_free(replacement_x_rlink);
					return 0;
				}
				//set Delete as the next operation
				nextAnnounceOp->opName=DELET;
				nextAnnounceOp->del.args.x=x;

				nextAnnounceOp->del.lv.x_llink=x->llink;//announce the value of x->llink to be used in CAS
				Node *hp1=nextAnnounceOp->del.lv.x_llink;//set hazard pointer
				if(nextAnnounceOp->del.lv.x_llink!=x->llink||nextAnnounceOp->del.lv.x_llink==0) goto label1;//check hazard pointer is set accurately and x->llink is not zero
				nextAnnounceOp->del.lv.x_llink_address=&x->llink;//announce the address of x->llink to be used in CAS

				//Following statements are on the same pattern as above i.e. announce the value of variable
				//set hazard pointer. check hazard pointer is set accurately. Announce the address of that variable

				nextAnnounceOp->del.lv.x_rlink=x->rlink;
				Node *hp2=nextAnnounceOp->del.lv.x_rlink;
				if(nextAnnounceOp->del.lv.x_rlink!=x->rlink||nextAnnounceOp->del.lv.x_rlink==0)  goto label1;
				nextAnnounceOp->del.lv.x_rlink_address=&x->rlink;

				nextAnnounceOp->del.lv.x_llink_rlink=nextAnnounceOp->del.lv.x_llink->rlink;
				Node *hp3=nextAnnounceOp->del.lv.x_llink_rlink;
				if(nextAnnounceOp->del.lv.x_llink_rlink!=nextAnnounceOp->del.lv.x_llink->rlink||nextAnnounceOp->del.lv.x_llink_rlink==0)  goto label1;
				nextAnnounceOp->del.lv.x_llink_rlink_address=&nextAnnounceOp->del.lv.x_llink->rlink;

				nextAnnounceOp->del.lv.x_rlink_llink=nextAnnounceOp->del.lv.x_rlink->llink;
				Node *hp4=nextAnnounceOp->del.lv.x_rlink_llink;
				if(nextAnnounceOp->del.lv.x_rlink_llink!=nextAnnounceOp->del.lv.x_rlink->llink||nextAnnounceOp->del.lv.x_rlink_llink==0)  goto label1;
				nextAnnounceOp->del.lv.x_rlink_llink_address=&nextAnnounceOp->del.lv.x_rlink->llink;

				nextAnnounceOp->del.lv.x_rlink_rlink=nextAnnounceOp->del.lv.x_rlink->rlink;
				Node *hp5=nextAnnounceOp->del.lv.x_rlink_rlink;
				if(nextAnnounceOp->del.lv.x_rlink_rlink!=nextAnnounceOp->del.lv.x_rlink->rlink||nextAnnounceOp->del.lv.x_rlink_rlink==0)  goto label1;
				nextAnnounceOp->del.lv.x_rlink_rlink_address=&nextAnnounceOp->del.lv.x_rlink->rlink;

				nextAnnounceOp->del.lv.x_llink_llink=nextAnnounceOp->del.lv.x_llink->llink;
				Node *hp6=nextAnnounceOp->del.lv.x_llink_llink;
				if(nextAnnounceOp->del.lv.x_llink_llink!=nextAnnounceOp->del.lv.x_llink->llink||nextAnnounceOp->del.lv.x_llink_llink==0)  goto label1;
				nextAnnounceOp->del.lv.x_llink_llink_address=&nextAnnounceOp->del.lv.x_llink->llink;

				nextAnnounceOp->del.lv.x_llink_llink_rlink=nextAnnounceOp->del.lv.x_llink_llink->rlink;
				Node *hp7=nextAnnounceOp->del.lv.x_llink_llink_rlink;
				if(nextAnnounceOp->del.lv.x_llink_llink_rlink!=nextAnnounceOp->del.lv.x_llink_llink->rlink||nextAnnounceOp->del.lv.x_llink_llink_rlink==0)  goto label1;
				nextAnnounceOp->del.lv.x_llink_llink_rlink_address=&nextAnnounceOp->del.lv.x_llink_llink->rlink;

				nextAnnounceOp->del.lv.x_rlink_rlink_llink=nextAnnounceOp->del.lv.x_rlink_rlink->llink;
				Node *hp8=nextAnnounceOp->del.lv.x_rlink_rlink_llink;
				if(nextAnnounceOp->del.lv.x_rlink_rlink_llink!=nextAnnounceOp->del.lv.x_rlink_rlink->llink||nextAnnounceOp->del.lv.x_rlink_rlink_llink==0)  goto label1;
				nextAnnounceOp->del.lv.x_rlink_rlink_llink_address=&nextAnnounceOp->del.lv.x_rlink_rlink->llink;

				nextAnnounceOp->del.lv.replacement_x_llink=replacement_x_llink;//announce the replacement for the node left to x
				nextAnnounceOp->del.lv.replacement_x_rlink=replacement_x_rlink;//announce the replacement for the node right to x
				replacement_x_llink->data=nextAnnounceOp->del.lv.x_llink->data;//copy data
				replacement_x_rlink->data=nextAnnounceOp->del.lv.x_rlink->data;//copy data
				//build the chain
	//x_llink_llink//replacement_x_llink//replacement_x_rlink//x_rlink_rlink
	//	---------	-------             --------               -------
	//	|		|	|	   |-----------|        |              |      |
	//	|		|===|      |===========|        |==============|      |
	//	 -------	-------             --------                -------
				replacement_x_llink->rlink=replacement_x_rlink;
				replacement_x_llink->llink=nextAnnounceOp->del.lv.x_llink_llink;
				replacement_x_rlink->llink=replacement_x_llink;
				replacement_x_rlink->rlink=nextAnnounceOp->del.lv.x_rlink_rlink;//x->rlink->rlink;

				//check addresses has not changed
				/*if(nextAnnounceOp->del.lv.x_llink_address!=&x->llink) goto label1;
				if(nextAnnounceOp->del.lv.x_rlink_address!=&x->rlink)  goto label1;
				if(nextAnnounceOp->del.lv.x_llink_rlink_address!=&x->llink->rlink)  goto label1;
				if(nextAnnounceOp->del.lv.x_rlink_llink_address!=&x->rlink->llink)  goto label1;
				if(nextAnnounceOp->del.lv.x_rlink_rlink_address!=&x->rlink->rlink)  goto label1;
				if(nextAnnounceOp->del.lv.x_llink_llink_address!=&x->llink->llink)  goto label1;
				if(nextAnnounceOp->del.lv.x_llink_llink_rlink_address!=&x->llink->llink->rlink)  goto label1;
				if(nextAnnounceOp->del.lv.x_rlink_rlink_llink_address!=&x->rlink->rlink->llink)  goto label1;*/

				//To announce the start of delete operation.
				void *v1=reinterpret_cast<void *>(nextAnnounceOp);
				void *v2=reinterpret_cast<void *>(curAnnouncedOp);
				void *res=(void *)InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&l.announce),(LONG)v1,(LONG)v2);
				if(res==v2)
				{
					//RetireNode(curAnnouncedOp);
					DeleteHelper(nextAnnounceOp);
					return 1;
				}
			}
			catch(...)
			{
				printf("Exception in Delete\n");
				_aligned_free(nextAnnounceOp);
				_aligned_free(replacement_x_llink);
				_aligned_free(replacement_x_rlink);
				return 0;
			}
		}
		else if(curAnnouncedOp->opName==INSERT)
		{
			InsertHelper(curAnnouncedOp);
		}
		else if(curAnnouncedOp->opName==DELET)
		{
			DeleteHelper(curAnnouncedOp);
		}
	}
label1:
	_aligned_free(nextAnnounceOp);
	_aligned_free(replacement_x_llink);
	_aligned_free(replacement_x_rlink);
	return 0;
}

//Second part of insert
void InsertHelper(AnnounceOp *curAnnouncedOp)
{
	//set x's right link to node p (newly created node)
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->insert.lv.x_rlink_address),(LONG)curAnnouncedOp->insert.args.p,(LONG)curAnnouncedOp->insert.lv.x_rlink);
	//set the left pointer of node next to x to point to p
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->insert.lv.x_rlink_llink_address),(LONG)curAnnouncedOp->insert.args.p,(LONG)curAnnouncedOp->insert.lv.x_rlink_llink);
	//To announce that insert operation is complete.
	AnnounceOp *nextAnnounceOp=(AnnounceOp*)_aligned_malloc(sizeof(struct AnnounceOp),InterLockedAlign );
	assert(nextAnnounceOp);
	nextAnnounceOp->opName=NONE;
	void *v1=reinterpret_cast<void>(nextAnnounceOp);
	void *v2=reinterpret_cast<void>(curAnnouncedOp);
	if(InterlockedCompareExchange(reinterpret_cast<volatile>(&l.announce),(LONG)v1,(LONG)v2)==(LONG)v2)
	{
		//RetireNode(v2);
	}
	else
	{
		_aligned_free(nextAnnounceOp);
	}
}

//Second part of delete
void DeleteHelper(AnnounceOp *curAnnouncedOp)
{
	//replace 2 nodes around x including x with two new nodes
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_llink_llink_rlink_address),(LONG)curAnnouncedOp->del.lv.replacement_x_llink,(LONG)curAnnouncedOp->del.lv.x_llink_llink_rlink);
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_rlink_rlink_llink_address),(LONG)curAnnouncedOp->del.lv.replacement_x_rlink,(LONG)curAnnouncedOp->del.lv.x_rlink_rlink_llink);
	//Set 3 retired nodes pointer fields to 0
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_llink_llink_address),(LONG)0,(LONG)curAnnouncedOp->del.lv.x_llink_llink);
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_llink_rlink_address),(LONG)0,(LONG)curAnnouncedOp->del.lv.x_llink_rlink);
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_rlink_rlink_address),(LONG)0,(LONG)curAnnouncedOp->del.lv.x_rlink_rlink);
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_rlink_llink_address),(LONG)0,(LONG)curAnnouncedOp->del.lv.x_rlink_llink);
	InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_llink_address),(LONG)0,(LONG)curAnnouncedOp->del.lv.x_llink);
	if(InterlockedCompareExchange(reinterpret_cast<volatile>(curAnnouncedOp->del.lv.x_rlink_address),(LONG)0,(LONG)curAnnouncedOp->del.lv.x_rlink)==(LONG)curAnnouncedOp->del.lv.x_rlink)
	{
		//RetireNode(x);
		//RetireNode(curAnnouncedOp->del.lv.x_llink);
		//RetireNode(curAnnouncedOp->del.lv.x_rlink);
	}
	//To announce that delete operation is complete.
	AnnounceOp *nextAnnounceOp=(AnnounceOp*)_aligned_malloc(sizeof(struct AnnounceOp),InterLockedAlign );
	assert(nextAnnounceOp);
	nextAnnounceOp->opName=NONE;
	void *v1=reinterpret_cast<void *>(nextAnnounceOp);
	void *v2=reinterpret_cast<void *>(curAnnouncedOp);
	if(InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&l.announce),(LONG)v1,(LONG)v2)==(LONG)v2)
	{
		//RetireNode(curAnnouncedOp);
	}
	else
	{
		_aligned_free(nextAnnounceOp);
	}
}


int main (void){

    return 0;
}
