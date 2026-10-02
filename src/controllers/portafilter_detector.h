#pragma once
#include <cstdint>
#include "../config/constants.h"

// Shot size a portafilter is used for. Values match the SINGLE/DOUBLE profile indices.
enum class ShotType : uint8_t {
    SINGLE = 0,
    DOUBLE = 1
};

// One learned physical portafilter: a 1-D weight cluster with running statistics.
struct PortafilterCluster {
    float mean_g;        // Running mean of the placement weight step
    float m2;            // Sum of squared deviations from the mean (Welford)
    uint16_t count;      // Effective sample count (capped at USER_PF_MAX_SAMPLES)
    uint8_t shot_type;   // ShotType
    uint8_t reserved;
};

enum class PortafilterMatch {
    MATCH,       // Unambiguous match to a learned portafilter
    AMBIGUOUS,   // Single and double clusters both fit about equally well
    NO_MATCH,    // Outside every learned portafilter's gate
    UNTRAINED    // Nothing learned yet
};

// How well a learned setup is separated from the nearest setup with the other label
enum class PortafilterSeparation {
    CLEAR,     // Gates don't overlap: always told apart
    CLOSE,     // Gates overlap: some placements will be asked about
    CONFLICT   // Each mean sits inside the other's gate: usually indistinguishable
};

struct PortafilterDetection {
    PortafilterMatch status = PortafilterMatch::UNTRAINED;
    int cluster_index = -1;
    ShotType shot_type = ShotType::SINGLE;
    float distance_sigmas = 0.0f;   // How far the weight is from the matched cluster mean
};

// Online clustering of portafilter weights.
//
// Every portafilter placed on the scale produces one weight-step sample. Each
// physical portafilter becomes its own cluster labeled SINGLE or DOUBLE, so any
// number of portafilters per shot size are supported. A sample matches the cluster
// it is closest to in units of that cluster's own spread (sigma), within a gate of
// USER_PF_MATCH_SIGMAS. Sigma blends the cluster's observed variance with a prior,
// so new clusters start with a sensible tolerance and tighten as they learn.
// Same-label clusters that grow into each other are merged.
//
// Persisted to NVS as a single blob. Runs on the UI task only.
class PortafilterDetector {
public:
    void init();

    PortafilterDetection classify(float weight_g) const;

    // Add a confirmed sample. With a valid hint the sample updates that cluster;
    // otherwise it joins the nearest same-label cluster that is the same physical
    // setup (see is_same_setup), or starts a new cluster. Returns the cluster index
    // the sample landed in and sets *created when a new cluster was made.
    int learn(float weight_g, ShotType shot_type, int hint_cluster = -1, bool* created = nullptr);

    // Strict "this weight is that physical setup" test: within max(USER_PF_SAME_SETUP_MIN_G,
    // 3x the cluster's *measured* sigma). Tighter than the match gate, which is padded
    // by the prior so AUTO tolerates placement noise and grounds residue.
    bool is_same_setup(int index, float weight_g) const;

    void forget(int index);
    void forget_all();

    int cluster_count() const { return cluster_count_; }
    const PortafilterCluster& cluster(int index) const { return clusters_[index]; }
    float cluster_sigma(int index) const;
    float cluster_gate(int index) const;
    float same_setup_radius(int index) const;

    // Worst separation between this setup and any setup with the other label.
    // nearest_other receives that setup's index (-1 if there is none).
    PortafilterSeparation separation(int index, int* nearest_other = nullptr) const;

    static const char* shot_type_name(ShotType shot_type);

private:
    static constexpr const char* kPrefsNamespace = "portafilter";
    static constexpr const char* kPrefsKey = "clusters";
    static constexpr uint8_t kBlobVersion = 1;

    struct StoredBlob {
        uint8_t version;
        uint8_t count;
        uint8_t reserved[2];
        PortafilterCluster clusters[USER_PF_MAX_CLUSTERS];
    };

    PortafilterCluster clusters_[USER_PF_MAX_CLUSTERS] = {};
    int cluster_count_ = 0;

    void load();
    void save() const;
    void add_sample(PortafilterCluster& cluster, float weight_g) const;
    void merge_overlapping(int index);
    void remove_at(int index);
    int nearest_cluster(float weight_g, ShotType shot_type, float* distance_sigmas) const;
};
