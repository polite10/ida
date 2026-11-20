// eigen_duman.cpp — vcpkg ile çözülen Eigen'in derlenip bağlandığını ve koştuğunu gösterir.
// Sayısal doğruluk sınaması değildir; yalnız bağımlılık zincirinin dumanıdır.

#include <Eigen/Dense>

#include <cstdio>

int main()
{
    Eigen::Matrix3d a;
    a << 4.0, 1.0, 0.0,
         1.0, 3.0, 1.0,
         0.0, 1.0, 2.0;
    const Eigen::Matrix3d birim = a * a.inverse();
    const double hata = (birim - Eigen::Matrix3d::Identity()).cwiseAbs().maxCoeff();
    std::printf("Eigen: |A*A^-1 - I|_max = %.3e\n", hata);
    return hata < 1e-12 ? 0 : 1;
}
