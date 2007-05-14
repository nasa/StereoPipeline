#include <boost/shared_ptr.hpp>

#include "StereoSessionKeypoint.h"

#include <vw/FileIO.h>
#include <vw/Image.h>
#include <vw/InterestPoint.h>

using namespace vw;
using namespace vw::ip;

#include "file_lib.h"
#include "SIFT.h"

using namespace vw;

void StereoSessionKeypoint::pre_preprocessing_hook(std::string const& input_file1, std::string const& input_file2,
                                                   std::string & output_file1, std::string & output_file2) {

  // Load the two images
  DiskImageView<PixelGray<float> > left_disk_image(m_left_image_file);
  DiskImageView<PixelGray<float> > right_disk_image(m_right_image_file);

  // Image Alignment
  //
  // Images are aligned by computing interest points, matching
  // them using a standard 2-Norm nearest-neighor metric, and then
  // rejecting outliers by fitting a similarity between the
  // putative matches using RANSAC.  
  HarrisInterest<float> harris;
  //LoGInterest<float> log;
  InterestThreshold<float> thresholder(0.00001);
  ScaledInterestPointDetector<float> detector(&harris, &thresholder);
  ImageOctaveHistory<ImageInterestData<float> > h1;
  ImageOctaveHistory<ImageInterestData<float> > h2;


  // Interest points are matched in image chunk of <= 2048x2048
  // pixels to conserve memory.
  vw_out(InfoMessage) << "\nInterest Point Detection:\n";
  static const int MAX_KEYPOINT_IMAGE_DIMENSION = 2048;
//   detector.record_history(&h1);
//   std::vector<InterestPoint> ip1 = interest_points(channels_to_planes(left_disk_image), detector, MAX_KEYPOINT_IMAGE_DIMENSION);
//   detector.record_history(&h2);
//   std::vector<InterestPoint> ip2 = interest_points(channels_to_planes(right_disk_image), detector, MAX_KEYPOINT_IMAGE_DIMENSION);

  // Old SIFT detector code.  Comment out the lines above and
  // uncomment these lines to enable. -mbroxton
  LoweDetector lowe;
  std::vector<InterestPoint> ip1 = interest_points(channels_to_planes(left_disk_image), lowe, MAX_KEYPOINT_IMAGE_DIMENSION);
  std::vector<InterestPoint> ip2 = interest_points(channels_to_planes(right_disk_image), lowe, MAX_KEYPOINT_IMAGE_DIMENSION);

  // Discard points beyond some number to keep matching time within reason.
  // Currently this is limited by the use of the patch descriptor.
  static const int NUM_POINTS = 800;
  vw_out(InfoMessage) << "Truncating to " << NUM_POINTS << " points:\n";
//   cull_interest_points(ip1, NUM_POINTS);
//   cull_interest_points(ip2, NUM_POINTS);

  // Generate descriptors for interest points.
  // TODO: Switch to SIFT descriptor
  vw_out(InfoMessage) << "Generating descriptors:\n";
  PatchDescriptor<float> desc;
  //SIFT_Descriptor<float> desc;
  ImageView<float> left = channels_to_planes(left_disk_image);
  ImageView<float> right = channels_to_planes(right_disk_image);
  generate_descriptors(ip1, left, desc);
  generate_descriptors(ip2, right, desc);
  //generate_descriptors(ip1, h1, desc);
  //generate_descriptors(ip2, h2, desc);
    
  // The basic interest point matcher does not impose any
  // constraints on the matched interest points.
  vw_out(InfoMessage) << "\nInterest Point Matching:\n";
  InterestPointMatcher<L2NormMetric,NullConstraint> matcher;
  std::vector<InterestPoint> matched_ip1, matched_ip2;
  matcher.match(ip1, ip2, matched_ip1, matched_ip2);
  vw_out(InfoMessage) << "Found " << matched_ip1.size() << " putative matches.\n";
  
  // RANSAC is used to fit a similarity transform between the
  // matched sets of points
  Matrix<double> align_matrix = ransac(matched_ip2, matched_ip1, 
                                       vw::math::SimilarityFittingFunctor(),
                                       KeypointErrorMetric());
  write_matrix(m_out_prefix + "-align.exr", align_matrix);

  ImageViewRef<PixelGray<float> > Limg = left_disk_image;
  ImageViewRef<PixelGray<float> > Rimg = transform(right_disk_image, HomographyTransform(align_matrix),
                                                   left_disk_image.cols(), left_disk_image.rows());

  output_file1 = m_out_prefix + "-L.tif";
  output_file2 = m_out_prefix + "-R.tif";

  write_image(output_file1, channel_cast_rescale<uint8>(Limg));
  write_image(output_file2, channel_cast_rescale<uint8>(Rimg)); 
}