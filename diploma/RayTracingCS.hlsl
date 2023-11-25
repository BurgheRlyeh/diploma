#define LIMIT_V 1013
#define LIMIT_I 1107

cbuffer VtsConstBuffer: register(b0) {
    float4 vertices[LIMIT_V];
}

cbuffer IdsConstBuffer: register(b1) {
    int4 indices[LIMIT_I];
}

cbuffer ModelBuffer: register(b2) {
    float4x4 mModel;
    float4x4 mModelInv;
    float4 posAngle;
}

cbuffer TrianglesIDs: register(b3) {
    int4 triIdx[LIMIT_I];
}

cbuffer RTBuffer: register(b4) {
    float4 whnf;
    float4x4 vpInv;
    int4 instsAlgLeafsTCheck;
    float4 camDir;
    int4 highlights;
}

struct AABB {
    float4 bmin, bmax;
};

struct BVHNode {
    AABB bb;
    int4 leftCntPar;
};

StructuredBuffer<BVHNode> nodes: register(t0);

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

        for (int i = 0; i < LIMIT_I; ++i) {
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
        int mId = triIdx[nodes[nodeId].leftCntPar.x + i].x / LIMIT_I;
        int tId = triIdx[nodes[nodeId].leftCntPar.x + i].x % LIMIT_I;

        Ray mRay;
        mRay.orig = mul(mModelInv, ray.orig);
        mRay.dest = mul(mModelInv, ray.dest);
        mRay.dir = normalize(mRay.dest - mRay.orig);

        float4 v0 = vertices[indices[tId].x];
        float4 v1 = vertices[indices[tId].y];
        float4 v2 = vertices[indices[tId].z];

        Intsec curr = rayTriangleIntersection(mRay, v0, v1, v2);

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
    float tLeft = rayIntersectsAABB(ray, nodes[nodeId].bb);

    int right = nodes[nodeId].leftCntPar.x + 1;
    float tRight = rayIntersectsAABB(ray, nodes[nodeId].bb);

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
            if (t == whnf.w || instsAlgLeafsTCheck.w == 1 && best.t + 1e6f < t)
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
            if (t == whnf.w || instsAlgLeafsTCheck.w == 1 && best.t + 1e6f < t)
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

[numthreads(1, 1, 1)]
void main(uint3 DTid: SV_DispatchThreadID) {
    Ray ray = generateRay(DTid.xy);

    Intsec best;
    switch (instsAlgLeafsTCheck.y) {
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
    
    if (best.t <= whnf.z || whnf.w <= best.t)
        return;
    
    if (best.tId == highlights.x) {
        texOutput[DTid.xy] = float4(1.f, 0.f, 0.f, 0.f);
        return;
    }
    
    if (best.tId == highlights.y) {
        texOutput[DTid.xy] = float4(0.f, 1.f, 0.f, 0.f);
        return;
    }
    
    if (best.tId == highlights.z) {
        texOutput[DTid.xy] = float4(0.f, 0.f, 1.f, 0.f);
        return;
    }
    
    // depth view
    float depth = best.t * dot(ray.dir, camDir);
        
    float4 colorNear = float4(1.f, 1.f, 1.f, 1.f);
    float4 colorFar = float4(0.f, 0.f, 0.f, 1.f);
        
    texOutput[DTid.xy] = lerp(colorNear, colorFar, 1.f - 1.f / depth);
}