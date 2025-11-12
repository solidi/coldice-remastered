#ifndef FLAMESYS_HEADER
#define FLAMESYS_HEADER

#include "r_studioint.h"

typedef struct FlameSys
{
	int MaxFlames;
	int Enable;
	float NextTick;
	float NextDlight;

} FlameSys;

// Minimal C++98-compatible int -> FlameSys associative container.
// Simple singly-linked-list implementation providing operator[], erase and find.
class MapIntFlameSys {
public:
    struct Node {
        int key;
        FlameSys value;
        Node *next;
        Node(int k) : key(k), next(NULL) {
            value.MaxFlames = 0;
            value.Enable = 0;
            value.NextTick = 0.0f;
            value.NextDlight = 0.0f;
        }
    };

    MapIntFlameSys() : head(NULL) {}
    ~MapIntFlameSys() { clear(); }

    // similar to std::map::operator[]: returns a reference, creates entry if missing
    FlameSys &operator[](int k) {
        Node *n = head;
        while (n) {
            if (n->key == k) return n->value;
            n = n->next;
        }
        Node *nn = new Node(k);
        nn->next = head;
        head = nn;
        return nn->value;
    }

    // erase key, return true if removed
    bool erase(int k) {
        Node **p = &head;
        while (*p) {
            if ((*p)->key == k) {
                Node *del = *p;
                *p = del->next;
                delete del;
                return true;
            }
            p = &((*p)->next);
        }
        return false;
    }

    // find: return pointer to value or NULL if not found
    FlameSys *find(int k) {
        Node *n = head;
        while (n) {
            if (n->key == k) return &n->value;
            n = n->next;
        }
        return NULL;
    }

    void clear() {
        Node *n = head;
        while (n) {
            Node *t = n->next;
            delete n;
            n = t;
        }
        head = NULL;
    }

private:
    Node *head;
};

class CFlameSystem
{
public:
	void SetState(int EntIndex, int State);
	void Extinguish(int EntIndex);
	void Process(cl_entity_s *Entity, const engine_studio_api_t &IEngineStudio);
	void Init();
private:
    MapIntFlameSys Data;
};

extern CFlameSystem FlameSystem;

#else
#endif;
