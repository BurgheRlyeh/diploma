#define LIMIT_V 1013
#define LIMIT_I 1107

#define MAX_LEVEL 15
#define MAX_STACK 4

cbuffer ModelBuffer: register(b0) {
    int4 primsCnt;
    float4x4 mModel;
    float4x4 mModelInv;
    float4 posAngle;
}

cbuffer RTBuffer: register(b1) {
    float4 whnf;
    float4x4 vpInv;
    int4 instsAlgLeafsTCheck;
    float4 camDir;
    int4 highlights;
}

StructuredBuffer<float4> vertices : register(t0);

StructuredBuffer<int4> indices : register(t1);

StructuredBuffer<uint4> triIdx : register(t2);

struct AABB {
    float4 bmin, bmax;
};

struct BVHNode {
    AABB bb;
    int4 leftCntPar;
};

StructuredBuffer<BVHNode> nodes: register(t3);

struct Ray {
    float4 orig;
    float4 dest;
    float4 dir;
};

struct Intsec {
    int mId;
    int tId;
    float t;
    float u;
    float v;
};

RWTexture2D<float4> texOutput: register(u0);

float4 pixelToWorld(float2 pixel, float depth) {
    float2 ndc = 2.f * pixel / whnf.xy - 1.f;
    ndc.y *= -1;

    depth = (1.f - depth) / (whnf.z - whnf.w);

    float4 res = mul(vpInv, float4(ndc, depth, 1.f));
    return res / res.w;
}

Ray generateRay(float2 screenPoint) {
    Ray ray;

    ray.orig = pixelToWorld(screenPoint.xy + 0.5f, 0.f);
    ray.dest = pixelToWorld(screenPoint.xy + 0.5f, 1.f);
    ray.dir = normalize(ray.dest - ray.orig);

    return ray;
}

// Moller-Trumbore Intersection Algorithm
Intsec rayTriangleIntersection(Ray ray, float4 v0, float4 v1, float4 v2) {
    Intsec intsec;
    intsec.mId = intsec.tId = -1;
    intsec.t = intsec.u = intsec.v = -1.f;

    // edges
    float3 e1 = v1.xyz - v0.xyz;
    float3 e2 = v2.xyz - v0.xyz;

    float3 h = cross(ray.dir.xyz, e2);
    float a = dot(e1, h);

    // check is parallel
    if (abs(a) < 1e-8)
        return intsec;

    float3 s = ray.orig.xyz - v0.xyz;
    intsec.u = dot(s, h) / a;

    // check u range
    if (intsec.u < 0.0 || 1.0 < intsec.u)
        return intsec;

    float3 q = cross(s, e1);
    intsec.v = dot(ray.dir.xyz, q) / a;

    // check v + u range
    if (intsec.v < 0.0 || 1.0 < intsec.u + intsec.v)
        return intsec;

    intsec.t = dot(e2, q) / a;
    return intsec;
}

// naive intersection part
Intsec naiveIntersection(Ray ray) {
    Intsec best;
    best.mId = best.tId = -1;
    best.t = whnf.w;
    best.u = best.v = -1.f;

    for (int m = 0; m < instsAlgLeafsTCheck.x; ++m) {
        Ray mRay;
        mRay.orig = mul(mModelInv, ray.orig);
        mRay.dest = mul(mModelInv, ray.dest);
        mRay.dir = normalize(mRay.dest - mRay.orig);

        for (int i = 0; i < primsCnt.x; ++i)
        {
            float4 v0 = vertices[indices[i].x];
            float4 v1 = vertices[indices[i].y];
            float4 v2 = vertices[indices[i].z];

            Intsec curr = rayTriangleIntersection(mRay, v0, v1, v2);

            if (whnf.z < curr.t && curr.t < best.t) {
                best = curr;
                best.mId = m;
                best.tId = i;
            }
        }
    }

    return best;
}

// bvh intersection part
float rayIntersectsAABB(Ray ray, AABB aabb) {
    float4 v1 = (aabb.bmin - ray.orig) / ray.dir;
    float4 v2 = (aabb.bmax - ray.orig) / ray.dir;

    float3 vmin = min(v1.xyz, v2.xyz);
    float3 vmax = max(v1.xyz, v2.xyz);

    float tmin = max(max(vmin.x, vmin.y), vmin.z);
    float tmax = min(min(vmax.x, vmax.y), vmax.z);

    return whnf.z < tmax && tmin <= tmax && tmin < whnf.w ? tmin : whnf.w;
}

Intsec bestBVHLeafIntersection(Ray ray, int nodeId) {
    Intsec best;
    best.mId = best.tId = -1;
    best.t = whnf.w;
    best.u = best.v = -1.f;

    
    for (int i = 0; i < nodes[nodeId].leftCntPar.y; ++i) {
        int mId = triIdx[nodes[nodeId].leftCntPar.x + i].x / primsCnt.x;
        int tId = triIdx[nodes[nodeId].leftCntPar.x + i].x % primsCnt.x;

        Ray mRay;
        mRay.orig = mul(mModelInv, ray.orig);
        mRay.dest = mul(mModelInv, ray.dest);
        mRay.dir = normalize(mRay.dest - mRay.orig);

        float4 v0 = vertices[indices[tId].x];
        float4 v1 = vertices[indices[tId].y];
        float4 v2 = vertices[indices[tId].z];

        Intsec curr = rayTriangleIntersection(mRay, v0, v1, v2);
        curr.t = mul(mModel, curr.t);

        if (whnf.z < curr.t && curr.t < best.t) {
            best = curr;
            best.mId = mId;
            best.tId = tId;
        }
    }

    return best;
}

Intsec bvhIntersection(Ray ray) {
    Intsec best;
    best.mId = best.tId = -1;
    best.t = whnf.w;
    best.u = best.v = -1.f;

    // Create a stack to store the nodes to be processed.
    int stack[599];
    int stackSize = 0;
    stack[stackSize++] = 0;

    while (stackSize > 0) {
        int nodeId = stack[--stackSize];
        
        if (rayIntersectsAABB(ray, nodes[nodeId].bb) == whnf.w)
            continue;

        if (nodes[nodeId].leftCntPar.y == 0) {
            stack[stackSize++] = nodes[nodeId].leftCntPar.x;
            stack[stackSize++] = nodes[nodeId].leftCntPar.x + 1;
            continue;
        }

        Intsec curr = bestBVHLeafIntersection(ray, nodeId);
        if (curr.t < best.t)
            best = curr;
    }

    return best;
}

// stack-less
int parent(int nodeId) {
    return nodes[nodeId].leftCntPar.z;
}

int sibling(int nodeId) {
    int parent = nodes[nodeId].leftCntPar.z;
    int left = nodes[parent].leftCntPar.x;
    int right = nodes[parent].leftCntPar.x + 1;
    return nodeId != left ? left : right;
}

int nearChild(int nodeId, Ray ray) {
    int left = nodes[nodeId].leftCntPar.x;

    int right = nodes[nodeId].leftCntPar.x + 1;
    // TODO wtf?
    float tLeft = rayIntersectsAABB(ray, nodes[nodeId].bb);
    float tRight = rayIntersectsAABB(ray, nodes[nodeId].bb);
    
    //float tLeft = rayIntersectsAABB(ray, nodes[left].bb);
    //float tRight = rayIntersectsAABB(ray, nodes[right].bb);

    return tLeft <= tRight ? left : right;
}

bool isLeaf(int nodeId) {
    return nodes[nodeId].leftCntPar.y > 0;
}

Intsec bvhStacklessIntersection(Ray ray) {
    Intsec best;
    best.mId = best.tId = -1;
    best.t = whnf.w;
    best.u = best.v = -1.f;

    if (rayIntersectsAABB(ray, nodes[0].bb) == whnf.w)
        return best;

    for (int2 nodeState = int2(nearChild(0, ray), 0); nodeState.x != 0;) {
        // from parent
        if (nodeState.y == 0) {
            float t = rayIntersectsAABB(ray, nodes[nodeState.x].bb);
            if (t == whnf.w || instsAlgLeafsTCheck.w == 1 && best.t + 1e-6f < t)
                nodeState = int2(sibling(nodeState.x), 1);
            else {
                if (!isLeaf(nodeState.x))
                    nodeState = int2(nearChild(nodeState.x, ray), 0);
                else {
                    if (instsAlgLeafsTCheck.z == 1) {
                        Intsec intsec = bestBVHLeafIntersection(ray, nodeState.x);
                        if (intsec.t < best.t)
                            best = intsec;
                    }
                    nodeState = int2(sibling(nodeState.x), 1);
                }
            }
        }
        // from sibling
        else if (nodeState.y == 1) {
            float t = rayIntersectsAABB(ray, nodes[nodeState.x].bb);
            if (t == whnf.w || instsAlgLeafsTCheck.w == 1 && best.t + 1e-6f < t)
                nodeState = int2(parent(nodeState.x), 2);
            else if (!isLeaf(nodeState.x))
                nodeState = int2(nearChild(nodeState.x, ray), 0);
            else {
                if (instsAlgLeafsTCheck.z == 1) {
                    Intsec intsec = bestBVHLeafIntersection(ray, nodeState.x);
                    if (intsec.t < best.t)
                        best = intsec;
                }

                nodeState = int2(parent(nodeState.x), 2);
            }
        }
        // from child
        else if (nodeState.y == 2) {
            if (nodeState.x == nearChild(parent(nodeState.x), ray))
                nodeState = int2(sibling(nodeState.x), 1);
            else
                nodeState = int2(parent(nodeState.x), 2);
        }
    }

    return best;
}

struct NodeIntsec {
    int nodeId;
    float t;
};

void insertSort(inout NodeIntsec a[4], int length) {
    for (int i = 1; i < length; i++) {
        NodeIntsec value = a[i];
        int j;
        for (j = i - 1; j >= 0 && a[j].t > value.t; j--)
            a[j + 1] = a[j];
        a[j + 1] = value;
    }
}

int nearChildQBVH(int nodeId, Ray ray, inout int visited) {
    //return nodes[nodeId].leftCntPar.x;
    
    BVHNode node = nodes[nodeId];
    
    NodeIntsec c[4];
    for (int i = 0; i < 4; ++i) {
        c[i].nodeId = -1;
        c[i].t = whnf.w;
    }
    int cSize = 0;
    
    for (int cnt = 0; cnt < node.leftCntPar.w; ++cnt) {
        int childId = node.leftCntPar.x + cnt;
        
        float t = rayIntersectsAABB(ray, nodes[childId].bb);
        if (t < whnf.w) {
            c[cSize].nodeId = childId;
            c[cSize].t = t;
            ++cSize;
        }
    }
    
    if (cSize == 0)
        return nodes[nodeId].leftCntPar.x;
    
    insertSort(c, cSize);
    int val = c[visited].nodeId;
    if (visited == cSize - 1) {
        visited = 4;
    }
    return val;
}

int siblingQBVH(int nodeId, Ray ray, inout int visited)
{
    int parent = nodes[nodeId].leftCntPar.z;
    return nearChildQBVH(parent, ray, visited);
    
    int sib = nodeId + 1;
    if (nodes[parent].leftCntPar.x + nodes[parent].leftCntPar.w == sib)
        return -1;
    return sib;
}

Intsec bvhStacklessIntersectionQBVH(Ray ray)
{
    Intsec best;
    best.mId = best.tId = -1;
    best.t = whnf.w;
    best.u = best.v = -1.f;

    if (rayIntersectsAABB(ray, nodes[0].bb) == whnf.w)
        return best;
    
    int trail[MAX_LEVEL];
    for (int i = 0; i < MAX_LEVEL; ++i)
        trail[i] = 0;
    int level = 1;

    for (int2 nodeState = int2(nearChildQBVH(0, ray, trail[level]), 0), iters = int2(0, 0); nodeState.x != 0 && iters.x < 10000; ++iters.x)
    {
        // from parent
        if (nodeState.y == 0) {
            float t = rayIntersectsAABB(ray, nodes[nodeState.x].bb);
            if (t == whnf.w || instsAlgLeafsTCheck.w == 1 && best.t + 1e-6f < t)
            {
                if (++trail[level] >= 4)
                {
                    --level;
                    nodeState = int2(parent(nodeState.x), 1);
                }
                else
                    nodeState = int2(siblingQBVH(nodeState.x, ray, trail[level]), 2);
            }
            // not leaf
            else if (!isLeaf(nodeState.x)) {
                trail[++level] = 0;
                nodeState = int2(nearChildQBVH(nodeState.x, ray, trail[level]), 0);
            }
            else {
                if (instsAlgLeafsTCheck.z == 1) {
                    Intsec intsec = bestBVHLeafIntersection(ray, nodeState.x);
                    if (intsec.t < best.t)
                        best = intsec;
                }
                    
                if (++trail[level] >= 4) {
                    --level;
                    nodeState = int2(parent(nodeState.x), 1);
                }
                else
                    nodeState = int2(siblingQBVH(nodeState.x, ray, trail[level]), 2);
            }
        }
        // from child
        else if (nodeState.y == 1)
        {
            if (++trail[level] >= 4)
            {
                --level;
                nodeState = int2(parent(nodeState.x), 1);
            }
            else
                nodeState = int2(siblingQBVH(nodeState.x, ray, trail[level]), 2);
        }
        // from sibling
        else if (nodeState.y == 2) {
            float t = rayIntersectsAABB(ray, nodes[nodeState.x].bb);
            if (t == whnf.w || instsAlgLeafsTCheck.w == 1 && best.t + 1e-6f < t) {
                if (++trail[level] >= 4)
                {
                    --level;
                    nodeState = int2(parent(nodeState.x), 1);
                }
                else
                    nodeState = int2(siblingQBVH(nodeState.x, ray, trail[level]), 2);
            }
            else if (!isLeaf(nodeState.x)) {
                trail[++level] = 0;
                nodeState = int2(nearChildQBVH(nodeState.x, ray, trail[level]), 0);
            }
            else
            {
                if (instsAlgLeafsTCheck.z == 1)
                {
                    Intsec intsec = bestBVHLeafIntersection(ray, nodeState.x);
                    if (intsec.t < best.t)
                        best = intsec;
                }
                
                if (++trail[level] >= 4)
                {
                    --level;
                    nodeState = int2(parent(nodeState.x), 1);
                }
                else
                    nodeState = int2(siblingQBVH(nodeState.x, ray, trail[level]), 2);
            }
        }
    }

    return best;
}

//struct NodeIntsec {
//    int nodeId = 0;
//    float t = whnf.w;
//    bool isParent = false;
//    bool isLastChild = false;
//};

//struct ShortStack {
//    NodeIntsec stack[MAX_STACK];
//    int size = 0;
//    int first = 0;
    
//    void pushOne(NodeIntsec val) {
//        if (size < MAX_STACK) {
//            stack[size++] = val;
//            return;
//        }
        
//        stack[first++] = val;
//        if (first == MAX_STACK)
//            first = 0;
//    }
//};

//void insertSort(inout NodeIntsec a[4], int length) {
//    for (int i = 1; i < length; i++) {
//        NodeIntsec value = a[i];
//        int j;
//        for (j = i - 1; j >= 0 && a[j].t > value.t; j--)
//            a[j + 1] = a[j];
//        a[j + 1] = value;
//    }
//}

//int findNextParentLevel(int trail[MAX_LEVEL], int level) {
//    for (int i = level - 1; 0 <= i; --i)
//        if (trail[i] != 4)
//            return i;
//    return -1;
//}

//bool pop(inout NodeIntsec nodeIntsec, inout ShortStack shortStack, inout int trail[MAX_LEVEL], inout int level)
//{
//    int parentLevel = findNextParentLevel(trail, level);
//    if (parentLevel < 0)
//        return true;
//    ++trail[parentLevel];
//    for (int i = parentLevel + 1; i < MAX_LEVEL; ++i) 
//        trail[i] = 0;
//    if (shortStack.size == 0) {
//        nodeIntsec.nodeId = 0;
//        nodeIntsec.t = whnf.z;
//        nodeIntsec.isLastChild = nodeIntsec.isParent = false;
//        level = 0;
//    }
//    else {
//        nodeIntsec = shortStack.stack[--shortStack.size];
//        if (nodeIntsec.isLastChild) {
//            trail[parentLevel] = 4;
//        }
//        level = parentLevel + (nodeIntsec.isParent ? 0 : 1);
//    }
//    return false;
//}

//Intsec wideBvhTraversalWithShortStack(Ray ray) {
//    Intsec best;
//    best.mId = best.tId = -1;
//    best.t = whnf.w;
//    best.u = best.v = -1.f;
    
//    NodeIntsec nodeIntsec;
//    nodeIntsec.nodeId = 0;
//    nodeIntsec.t = best.t;
//    nodeIntsec.isParent = nodeIntsec.isLastChild = false;
    
//    int trail[MAX_LEVEL];
//    int level = 0;
    
//    ShortStack shortStack;
//    for (int i = 0; i < MAX_LEVEL; ++i)
//        shortStack.stack[i] = nodeIntsec;
//    shortStack.size = 0;
    
//    bool exit = false;
    
//    while (!exit) {
//        if (nodes[nodeIntsec.nodeId].leftCntPar.y) {
//            Intsec intsec = bestBVHLeafIntersection(ray, nodeIntsec.nodeId);
//            if (intsec.t < best.t)
//                best = intsec;
//            if (pop(nodeIntsec, shortStack, trail, level))
//                exit = true;
//            continue;
//        }
        
//        int k = trail[level];
        
//        NodeIntsec s[4];
//        int sizeS = 0;
        
//        for (int i = nodes[nodeIntsec.nodeId].leftCntPar.x, cnt = 0;
//            cnt < nodes[nodeIntsec.nodeId].leftCntPar.w; ++cnt, ++i) {
//            float t = rayIntersectsAABB(ray, nodes[i].bb);
//            if (t < best.t) {
//                s[sizeS].nodeId = i;
//                s[sizeS].t = t;
//                ++sizeS;
//            }
//        }
        
//        insertSort(s, sizeS);
        
//        NodeIntsec q[4];
//        int sizeQ = 0;
//        if (k == 4)
//            q[sizeQ++] = s[sizeS - 1];
//        else
//            for (int i = k; i < sizeS; ++i)
//                q[sizeQ++] = s[i];

//        if (sizeQ == 0 && pop(nodeIntsec, shortStack, trail, level))
//            exit = true;
//        else {
//            nodeIntsec = q[0];
//            if (sizeQ == 1)
//                trail[level] = 4;
//            else {
//                q[sizeQ - 1].isLastChild = true;
//                for (int i = sizeQ - 1; i >= 0; --i)
//                    shortStack.stack[shortStack.size++] = q[i];
//            }
//            ++level;
//        }
//       }
    
//    return best;
//}

[numthreads(1, 1, 1)]
void main(uint3 DTid: SV_DispatchThreadID) {
    Ray ray = generateRay(DTid.xy);

    Intsec best;
    if (nodes[0].leftCntPar.z == -1) {
        switch (instsAlgLeafsTCheck.y)
        {
            case 0:
                best = naiveIntersection(ray);
                break;
            case 1:
                best = bvhIntersection(ray);
                break;
            case 2:
                best = bvhStacklessIntersection(ray);
                break;
        }
    }
    else {
        best = bvhStacklessIntersectionQBVH(ray);
    }
    
    if (best.t <= whnf.z || whnf.w <= best.t)
        return;
    
    // depth view
    float depth = best.t * dot(ray.dir, camDir);
        
    float4 colorNear = float4(0.25f, 0.25f, 0.25f, 1.f);
    float4 colorFar = float4(0.75f, 0.75f, 0.75f, 1.f);
        
    float4 color = lerp(colorNear, colorFar, 1.f - 1.f / depth);
    
    if (triIdx[best.tId].z == 1) {
        color.x += (1.f - color.x) / 0.5f;
    }
    
    if (triIdx[best.tId].w == 1) {
        color.z += (1.f - color.z) / 0.5f;
    }
    
    //if (best.tId == highlights.x) {
    //    color.x += (1.f - color.x) / 4.f;
    //    color.yz -= (float2(1.f, 1.f) - color.yz) / 2.f;
    //}
    //if (best.tId == highlights.y) {
    //    color.y += (1.f - color.y) / 4.f;
    //    color.xz -= (float2(1.f, 1.f) - color.xz) / 2.f;
    //}
    //if (best.tId == highlights.z) {
    //    color.z += (1.f - color.z) / 4.f;
    //    color.xy -= (float2(1.f, 1.f) - color.xy) / 2.f;
    //}
    
    texOutput[DTid.xy] = color;
}