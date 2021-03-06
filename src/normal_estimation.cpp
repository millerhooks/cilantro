#include <cilantro/normal_estimation.hpp>
#include <cilantro/principal_component_analysis.hpp>

namespace cilantro {
    NormalEstimation::NormalEstimation(const std::vector<Eigen::Vector3f> &points)
            : input_cloud_(NULL),
              input_points_(&points),
              kd_tree_ptr_(new KDTree3D(points)),
              kd_tree_owned_(true),
              view_point_(Eigen::Vector3f::Zero())
    {}

    NormalEstimation::NormalEstimation(const std::vector<Eigen::Vector3f> &points, const KDTree3D &kd_tree)
            : input_cloud_(NULL),
              input_points_(&points),
              kd_tree_ptr_((KDTree3D*)&kd_tree),
              kd_tree_owned_(false),
              view_point_(Eigen::Vector3f::Zero())
    {}

    NormalEstimation::NormalEstimation(const PointCloud &cloud)
            : input_cloud_((PointCloud *)&cloud),
              input_points_(&cloud.points),
              kd_tree_ptr_(new KDTree3D(cloud.points)),
              kd_tree_owned_(true),
              view_point_(Eigen::Vector3f::Zero())
    {}

    NormalEstimation::NormalEstimation(const PointCloud &cloud, const KDTree3D &kd_tree)
            : input_cloud_((PointCloud *)&cloud),
              input_points_(&cloud.points),
              kd_tree_ptr_((KDTree3D*)&kd_tree),
              kd_tree_owned_(false),
              view_point_(Eigen::Vector3f::Zero())
    {}

    NormalEstimation::~NormalEstimation() {
        if (kd_tree_owned_) delete kd_tree_ptr_;
    }

    std::vector<Eigen::Vector3f> NormalEstimation::estimateNormalsKNN(size_t num_neighbors) const {
        std::vector<Eigen::Vector3f> normals(input_points_->size());

        Eigen::Vector3f nan(Eigen::Vector3f::Constant(std::numeric_limits<float>::quiet_NaN()));
        if (input_points_->size() < 3) {
            for (size_t i = 0; i < normals.size(); i++) {
                normals[i] = nan;
            }
            return normals;
        }

#pragma omp parallel for shared (normals)
        for (size_t i = 0; i < input_points_->size(); i++) {
            std::vector<size_t> neighbors;
            std::vector<float> distances;
            kd_tree_ptr_->kNNSearch((*input_points_)[i], num_neighbors, neighbors, distances);

            std::vector<Eigen::Vector3f> neighborhood(neighbors.size());
            for (size_t j = 0; j < neighbors.size(); j++) {
                neighborhood[j] = (*input_points_)[neighbors[j]];
            }

            PrincipalComponentAnalysis3D pca(neighborhood);
            normals[i] = pca.getEigenVectorsMatrix().col(2);

//        (*input_points_)[i] = pca.reconstruct<2>(pca.project<2>((*input_points_)[i]));

            if (normals[i].dot(view_point_ - (*input_points_)[i]) < 0.0f) {
                normals[i] *= -1.0f;
            }
        }

        return normals;
    }

    void NormalEstimation::estimateNormalsInPlaceKNN(size_t num_neighbors) const {
        if (input_cloud_ == NULL) return;
        input_cloud_->normals = estimateNormalsKNN(num_neighbors);
    }

    std::vector<Eigen::Vector3f> NormalEstimation::estimateNormalsRadius(float radius) const {
        float radius_sq = radius*radius;

        std::vector<Eigen::Vector3f> normals(input_points_->size());

        Eigen::Vector3f nan(Eigen::Vector3f::Constant(std::numeric_limits<float>::quiet_NaN()));

#pragma omp parallel for shared (normals)
        for (size_t i = 0; i < input_points_->size(); i++) {
            std::vector<size_t> neighbors;
            std::vector<float> distances;
            kd_tree_ptr_->radiusSearch((*input_points_)[i], radius_sq, neighbors, distances);

            if (neighbors.size() < 3) {
                normals[i] = nan;
                continue;
            }

            std::vector<Eigen::Vector3f> neighborhood(neighbors.size());
            for (size_t j = 0; j < neighbors.size(); j++) {
                neighborhood[j] = (*input_points_)[neighbors[j]];
            }

            PrincipalComponentAnalysis3D pca(neighborhood);
            normals[i] = pca.getEigenVectorsMatrix().col(2);

//        (*input_points_)[i] = pca.reconstruct<2>(pca.project<2>((*input_points_)[i]));

            if (normals[i].dot(view_point_ - (*input_points_)[i]) < 0.0f) {
                normals[i] *= -1.0f;
            }
        }

        return normals;
    }

    void NormalEstimation::estimateNormalsInPlaceRadius(float radius) const {
        if (input_cloud_ == NULL) return;
        input_cloud_->normals = estimateNormalsRadius(radius);
    }

    std::vector<Eigen::Vector3f> NormalEstimation::estimateNormalsKNNInRadius(size_t k, float radius) const {
        float radius_sq = radius*radius;

        std::vector<Eigen::Vector3f> normals(input_points_->size());

        Eigen::Vector3f nan(Eigen::Vector3f::Constant(std::numeric_limits<float>::quiet_NaN()));

#pragma omp parallel for shared (normals)
        for (size_t i = 0; i < input_points_->size(); i++) {
            std::vector<size_t> neighbors;
            std::vector<float> distances;
            kd_tree_ptr_->kNNInRadiusSearch((*input_points_)[i], k, radius_sq, neighbors, distances);

            if (neighbors.size() < 3) {
                normals[i] = nan;
                continue;
            }

            std::vector<Eigen::Vector3f> neighborhood(neighbors.size());
            for (size_t j = 0; j < neighbors.size(); j++) {
                neighborhood[j] = (*input_points_)[neighbors[j]];
            }

            PrincipalComponentAnalysis3D pca(neighborhood);
            normals[i] = pca.getEigenVectorsMatrix().col(2);

//        (*input_points_)[i] = pca.reconstruct<2>(pca.project<2>((*input_points_)[i]));

            if (normals[i].dot(view_point_ - (*input_points_)[i]) < 0.0f) {
                normals[i] *= -1.0f;
            }
        }

        return normals;
    }

    void NormalEstimation::estimateNormalsInPlaceKNNInRadius(size_t k, float radius) const {
        if (input_cloud_ == NULL) return;
        input_cloud_->normals = estimateNormalsKNNInRadius(k, radius);
    }

    std::vector<Eigen::Vector3f> NormalEstimation::estimateNormals(const KDTree3D::Neighborhood &nh) const {
        switch (nh.type) {
            case KDTree3D::NeighborhoodType::KNN:
                return estimateNormalsKNN(nh.maxNumberOfNeighbors);
            case KDTree3D::NeighborhoodType::RADIUS:
                return estimateNormalsRadius(nh.radius);
            case KDTree3D::NeighborhoodType::KNN_IN_RADIUS:
                return estimateNormalsKNNInRadius(nh.maxNumberOfNeighbors, nh.radius);
        }
    }

    void NormalEstimation::estimateNormalsInPlace(const KDTree3D::Neighborhood &nh) const {
        if (input_cloud_ == NULL) return;
        input_cloud_->normals = estimateNormals(nh);
    }
}
