#include "portafilter_detector.h"

#include <cmath>
#include <cstring>
#include "preferences_idf.h"
#include <cstdio>

void PortafilterDetector::init() {
    load();
}

PortafilterDetection PortafilterDetector::classify(float weight_g) const {
    PortafilterDetection result;
    if (cluster_count_ == 0 || !std::isfinite(weight_g)) {
        result.status = PortafilterMatch::UNTRAINED;
        return result;
    }

    int best = -1;
    float best_z = 0.0f;
    int nearest = -1;
    float nearest_z = 0.0f;

    for (int i = 0; i < cluster_count_; ++i) {
        const float distance_g = std::fabs(weight_g - clusters_[i].mean_g);
        const float z = distance_g / cluster_sigma(i);
        if (nearest < 0 || z < nearest_z) {
            nearest = i;
            nearest_z = z;
        }
        if (distance_g <= cluster_gate(i) && (best < 0 || z < best_z)) {
            best = i;
            best_z = z;
        }
    }

    if (best < 0) {
        result.status = PortafilterMatch::NO_MATCH;
        result.cluster_index = nearest;
        result.distance_sigmas = nearest_z;
        return result;
    }

    result.cluster_index = best;
    result.shot_type = static_cast<ShotType>(clusters_[best].shot_type);
    result.distance_sigmas = best_z;
    result.status = PortafilterMatch::MATCH;

    // A competing match with the other label that fits nearly as well can't be told apart
    for (int i = 0; i < cluster_count_; ++i) {
        if (clusters_[i].shot_type == clusters_[best].shot_type) {
            continue;
        }
        const float distance_g = std::fabs(weight_g - clusters_[i].mean_g);
        const float z = distance_g / cluster_sigma(i);
        if (distance_g <= cluster_gate(i) && (z - best_z) < USER_PF_AMBIGUITY_MARGIN_SIGMAS) {
            result.status = PortafilterMatch::AMBIGUOUS;
            break;
        }
    }
    return result;
}

int PortafilterDetector::learn(float weight_g, ShotType shot_type, int hint_cluster) {
    if (!std::isfinite(weight_g) || weight_g <= 0.0f) {
        return -1;
    }

    int index = -1;
    if (hint_cluster >= 0 && hint_cluster < cluster_count_ &&
        clusters_[hint_cluster].shot_type == static_cast<uint8_t>(shot_type)) {
        index = hint_cluster;
    } else {
        float distance_sigmas = 0.0f;
        const int nearest = nearest_cluster(weight_g, shot_type, &distance_sigmas);
        if (nearest >= 0 && std::fabs(weight_g - clusters_[nearest].mean_g) <= cluster_gate(nearest)) {
            index = nearest;
        }
    }

    if (index < 0) {
        // New portafilter. When full, replace the least-established one.
        if (cluster_count_ >= USER_PF_MAX_CLUSTERS) {
            int weakest = 0;
            for (int i = 1; i < cluster_count_; ++i) {
                if (clusters_[i].count < clusters_[weakest].count) {
                    weakest = i;
                }
            }
            remove_at(weakest);
        }
        index = cluster_count_++;
        clusters_[index] = {};
        clusters_[index].shot_type = static_cast<uint8_t>(shot_type);
    }

    add_sample(clusters_[index], weight_g);
    LOG_BLE("[PORTAFILTER] Learned %s sample %.2fg -> cluster %d (mean %.2fg, sigma %.2fg, n=%u)\n",
            shot_type_name(shot_type), static_cast<double>(weight_g), index,
            static_cast<double>(clusters_[index].mean_g), static_cast<double>(cluster_sigma(index)),
            static_cast<unsigned>(clusters_[index].count));

    merge_overlapping(index);
    save();
    return index;
}

void PortafilterDetector::forget(int index) {
    if (index < 0 || index >= cluster_count_) {
        return;
    }
    remove_at(index);
    save();
}

void PortafilterDetector::forget_all() {
    cluster_count_ = 0;
    std::memset(clusters_, 0, sizeof(clusters_));
    save();
}

float PortafilterDetector::cluster_sigma(int index) const {
    const PortafilterCluster& c = clusters_[index];
    const float prior_var = USER_PF_PRIOR_SIGMA_G * USER_PF_PRIOR_SIGMA_G;
    const float dof = (c.count > 0 ? static_cast<float>(c.count - 1) : 0.0f) + USER_PF_PRIOR_STRENGTH;
    const float variance = (c.m2 + USER_PF_PRIOR_STRENGTH * prior_var) / dof;
    return std::sqrt(variance);
}

float PortafilterDetector::cluster_gate(int index) const {
    float gate = USER_PF_MATCH_SIGMAS * cluster_sigma(index);
    if (gate < USER_PF_MIN_GATE_G) gate = USER_PF_MIN_GATE_G;
    if (gate > USER_PF_MAX_GATE_G) gate = USER_PF_MAX_GATE_G;
    return gate;
}

const char* PortafilterDetector::shot_type_name(ShotType shot_type) {
    return shot_type == ShotType::DOUBLE ? "DOUBLE" : "SINGLE";
}

void PortafilterDetector::add_sample(PortafilterCluster& cluster, float weight_g) const {
    // Fade old samples once the cap is reached so the cluster follows slow drift
    if (cluster.count >= USER_PF_MAX_SAMPLES) {
        cluster.m2 *= static_cast<float>(USER_PF_MAX_SAMPLES - 1) / cluster.count;
        cluster.count = USER_PF_MAX_SAMPLES - 1;
    }

    // Welford's online mean/variance update
    cluster.count++;
    const float delta = weight_g - cluster.mean_g;
    cluster.mean_g += delta / cluster.count;
    cluster.m2 += delta * (weight_g - cluster.mean_g);
}

void PortafilterDetector::merge_overlapping(int index) {
    for (int i = 0; i < cluster_count_; ++i) {
        if (i == index || clusters_[i].shot_type != clusters_[index].shot_type) {
            continue;
        }
        const float distance_g = std::fabs(clusters_[i].mean_g - clusters_[index].mean_g);
        const float smaller_gate = std::fmin(cluster_gate(i), cluster_gate(index));
        if (distance_g > std::fmax(USER_PF_MERGE_GATE_G, smaller_gate)) {
            continue;
        }

        // Combine the two clusters' statistics (parallel variance formula)
        PortafilterCluster& a = clusters_[index];
        const PortafilterCluster& b = clusters_[i];
        const float n_a = a.count;
        const float n_b = b.count;
        const float n = n_a + n_b;
        const float delta = b.mean_g - a.mean_g;
        a.mean_g += delta * n_b / n;
        a.m2 += b.m2 + delta * delta * n_a * n_b / n;
        a.count = static_cast<uint16_t>(n > USER_PF_MAX_SAMPLES ? USER_PF_MAX_SAMPLES : n);

        LOG_BLE("[PORTAFILTER] Merged overlapping %s clusters -> mean %.2fg\n",
                shot_type_name(static_cast<ShotType>(a.shot_type)), static_cast<double>(a.mean_g));
        remove_at(i);
        if (i < index) {
            index--;
        }
        i = -1;  // Rescan: the merged cluster's gate changed
    }
}

void PortafilterDetector::remove_at(int index) {
    for (int i = index; i < cluster_count_ - 1; ++i) {
        clusters_[i] = clusters_[i + 1];
    }
    cluster_count_--;
    clusters_[cluster_count_] = {};
}

int PortafilterDetector::nearest_cluster(float weight_g, ShotType shot_type, float* distance_sigmas) const {
    int nearest = -1;
    float nearest_z = 0.0f;
    for (int i = 0; i < cluster_count_; ++i) {
        if (clusters_[i].shot_type != static_cast<uint8_t>(shot_type)) {
            continue;
        }
        const float z = std::fabs(weight_g - clusters_[i].mean_g) / cluster_sigma(i);
        if (nearest < 0 || z < nearest_z) {
            nearest = i;
            nearest_z = z;
        }
    }
    if (distance_sigmas) {
        *distance_sigmas = nearest_z;
    }
    return nearest;
}

void PortafilterDetector::load() {
    cluster_count_ = 0;
    std::memset(clusters_, 0, sizeof(clusters_));

    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, true)) {
        return;
    }
    StoredBlob blob = {};
    const size_t read = prefs.isKey(kPrefsKey) ? prefs.getBytes(kPrefsKey, &blob, sizeof(blob)) : 0;
    prefs.end();

    if (read != sizeof(blob) || blob.version != kBlobVersion || blob.count > USER_PF_MAX_CLUSTERS) {
        return;
    }
    for (int i = 0; i < blob.count; ++i) {
        const PortafilterCluster& c = blob.clusters[i];
        if (c.count == 0 || !std::isfinite(c.mean_g) || !std::isfinite(c.m2) || c.m2 < 0.0f ||
            c.shot_type > static_cast<uint8_t>(ShotType::DOUBLE)) {
            continue;
        }
        clusters_[cluster_count_++] = c;
    }
    LOG_BLE("[PORTAFILTER] Loaded %d learned portafilter(s)\n", cluster_count_);
}

void PortafilterDetector::save() const {
    StoredBlob blob = {};
    blob.version = kBlobVersion;
    blob.count = static_cast<uint8_t>(cluster_count_);
    std::memcpy(blob.clusters, clusters_, sizeof(clusters_));

    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) {
        LOG_BLE("[PORTAFILTER] ERROR: could not open NVS namespace\n");
        return;
    }
    prefs.putBytes(kPrefsKey, &blob, sizeof(blob));
    prefs.end();
}
