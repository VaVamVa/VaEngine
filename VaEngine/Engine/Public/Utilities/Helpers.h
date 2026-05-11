#pragma once

#define SAFE_DELETE(p) { if(p) { delete (p); (p) = nullptr; } }
#define SAFE_DESTROY(p) { if(p) { (p)->Destroy(); (p) = nullptr; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = nullptr; } }

#define SAFE_DELETE_VALUE_ARRAY(p) { if(p) { delete[] (p); (p) = nullptr; } }
#define SAFE_DELETE_ARRAY_WITH_SIZE(p, size) { if(p) { for (size_t i = 0; i < (size); ++i) { SAFE_DELETE((p)[i]); } delete[] (p); (p) = nullptr; } }
#define SAFE_RELEASE_ARRAY(p) { if(p) { for (size_t i = 0; i < sizeof(p) / sizeof(p[0]); ++i) { (p)[i]->Release(); } delete[] (p); (p) = nullptr; } }
#define SAFE_DESTROY_ARRAY(p) { if(p) { for (size_t i = 0; i < sizeof(p) / sizeof(p[0]); ++i) { (p)[i]->Destroy(); } delete[] (p); (p) = nullptr; } }

#define SAFE_DELETE_VECTOR(vec) { for (auto& p : (vec)) { SAFE_DELETE(p); } (vec).clear(); }
#define SAFE_DESTROY_VECTOR(vec) { for (auto& p : (vec)) { SAFE_DESTROY(p); } (vec).clear(); }
#define SAFE_RELEASE_VECTOR(vec) { for (auto& p : (vec)) { SAFE_RELEASE(p); } (vec).clear(); }

