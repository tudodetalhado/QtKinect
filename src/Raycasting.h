#ifndef _RAYCASTING_H_
#define _RAYCASTING_H_


#include <iostream>
#include <chrono>
#include <sstream>
#include <fstream>
#include <vector>
#include <Eigen/Dense>
#include "Interpolator.hpp"
#include "RayBox.h"
#include "Timer.h"
#include "RayIntersection.h"

enum BoxFace
{
	Top = 0,
	Bottom,
	Front,
	Rear,
	Left,
	Right,
	Undefined
};

template <typename Type>
static BoxFace box_face_from_normal(const Eigen::Matrix<Type, 3, 1>& normal)
{
	if (normal.z() < -0.5f)	
		return BoxFace::Front;

	if (normal.z() > 0.5f)
		return BoxFace::Rear;
	
	if (normal.y() < -0.5f)
		return BoxFace::Bottom;

	if (normal.y() > 0.5f)
		return BoxFace::Top;

	if (normal.x() < -0.5f)
		return BoxFace::Left;

	if (normal.x() > 0.5f)
		return BoxFace::Right;

	return BoxFace::Undefined;
}

static std::string box_face_to_string(const BoxFace face)
{
	switch (face)
	{
	case BoxFace::Top: return "Top";
	case BoxFace::Bottom: return "Bottom";
	case BoxFace::Front: return "Front";
	case BoxFace::Rear: return "Rear";
	case BoxFace::Left: return "Left";
	case BoxFace::Right: return "Right";
	default:
	case BoxFace::Undefined: return "Undefined";
	}
}


static BoxFace box_face_in_face_out(const BoxFace& face_out)
{
	switch (face_out)
	{
	case BoxFace::Top: return BoxFace::Bottom;
	case BoxFace::Bottom: return BoxFace::Top;
	case BoxFace::Front: return BoxFace::Rear;
	case BoxFace::Rear: return BoxFace::Front;
	case BoxFace::Left: return BoxFace::Right;
	case BoxFace::Right: return BoxFace::Left;
	default:
	case BoxFace::Undefined: return BoxFace::Undefined;
	}
}



static int get_index_from_3d(const Eigen::Vector3f hit, const Eigen::Vector3i& voxel_count, const Eigen::Vector3i& voxel_size)
{
	const int max_x = voxel_count.x() * voxel_size.x();
	const int max_y = voxel_count.y() * voxel_size.y();
	const int max_z = voxel_count.z() * voxel_size.z();

	int x = (hit.x() < max_x) ? (int)hit.x() : max_x - 1;
	int y = (hit.y() < max_y) ? (int)hit.y() : max_y - 1;
	int z = (hit.z() < max_z) ? (int)hit.z() : max_z - 1;

	return z / voxel_size.z() * voxel_count.x() * voxel_count.y() + y / voxel_size.y() * voxel_count.y() + x / voxel_size.x();
}

static Eigen::Vector3i index_3d_from_array(
	int array_index,
	const Eigen::Vector3i& voxel_count,
	const Eigen::Vector3i& voxel_size)
{
	return Eigen::Vector3i(
		int(std::fmod(array_index, voxel_count.x())) * voxel_size.x(),
		int(std::fmod(array_index / voxel_count.y(), voxel_count.y())) * voxel_size.y(),
		int(array_index / (voxel_count.x() * voxel_count.y())) * voxel_size.z());
}


static Eigen::Vector3f eigen_clamp(Eigen::Vector3f v, float a, float b)
{
	return Eigen::Vector3f(
		std::fmax(a, std::fmin(b, v.x())),
		std::fmax(a, std::fmin(b, v.y())),
		std::fmax(a, std::fmin(b, v.z())));
}


static Eigen::Vector3f eigen_fminf(Eigen::Vector3f a, Eigen::Vector3f b)
{
	return Eigen::Vector3f(fminf(a[0], b[0]), fminf(a[1], b[1]), fminf(a[2], b[2]));
};


static Eigen::Vector3f eigen_fmaxf(Eigen::Vector3f a, Eigen::Vector3f b)
{
	return Eigen::Vector3f(fmaxf(a[0], b[0]), fmaxf(a[1], b[1]), fmaxf(a[2], b[2]));
};


static Eigen::Vector3f eigen_mulf(Eigen::Vector3f a, Eigen::Vector3f b)
{
	return Eigen::Vector3f(a[0] * b[0], a[1] * b[1], a[2] * b[2]);
};

static int intersectBox(Ray<float> r, Eigen::Vector3f boxmin, Eigen::Vector3f boxmax, float *tnear, float *tfar)
{
	// compute intersection of ray with all six bbox planes
	Eigen::Vector3f tbot = eigen_mulf(r.inv_direction, (boxmin - r.origin));
	Eigen::Vector3f ttop = eigen_mulf(r.inv_direction, (boxmax - r.origin));

	// re-order intersections to find smallest and largest on each axis
	Eigen::Vector3f tmin = eigen_fminf(ttop, tbot);
	Eigen::Vector3f tmax = eigen_fmaxf(ttop, tbot);

	// find the largest tmin and the smallest tmax
	float largest_tmin = fmaxf(fmaxf(tmin.x(), tmin.y()), fmaxf(tmin.x(), tmin.z()));
	float smallest_tmax = fminf(fminf(tmax.x(), tmax.y()), fminf(tmax.x(), tmax.z()));

	*tnear = largest_tmin;
	*tfar = smallest_tmax;

	return smallest_tmax > largest_tmin;
}



// 
// Face Index
// 0-Top, 1-Bottom, 2-Front, 3-Back, 4-Left, 5-Right
//
static int get_index_from_face(int face, int last_index, Eigen::Vector3i voxel_count)
{
	
	switch (face)
	{
		case 0: return last_index + voxel_count.x();					// Top
		case 1: return last_index - voxel_count.x();					// Bottom
		case 2: return last_index - voxel_count.x() * voxel_count.y();	// Front
		case 3: return last_index + voxel_count.x() * voxel_count.y();	// Back
		case 4: return last_index - 1;									// Left
		case 5: return last_index + 1;									// Right
		default: return -1;
	}
}





template <typename Type>
int face_intersections(
	const Eigen::Matrix<Type, 3, 1>& ray_origin,
	const Eigen::Matrix<Type, 3, 1>& ray_direction,
	const Eigen::Vector3i& voxel_count,
	const Eigen::Vector3i& voxel_size,
	const int voxel_index,
	const BoxFace& face_in,
	const Eigen::Matrix<Type, 3, 1>& hit_in,
	BoxFace& face_out,
	Eigen::Matrix<Type, 3, 1>& hit_out,
	int& next_voxel_index)
{
	struct FaceData
	{
		BoxFace face;
		Eigen::Vector3f hit;
		int voxel_index;
		float dist;
		FaceData(BoxFace f, Eigen::Vector3f ht, int vx, float ds) : face(f), hit(ht), voxel_index(vx), dist(ds){}
	};

	Eigen::Matrix<Type, 3, 1> hit;
	Eigen::Matrix<Type, 3, 1> voxel_pos = index_3d_from_array(voxel_index, voxel_count, voxel_size).cast<Type>();
	//voxel_pos += (voxel_size * 0.5f);	// only if using the center of face

	BoxFace face = BoxFace::Undefined;
	std::vector<FaceData> face_list;

	face = BoxFace::Top;
	{
		Eigen::Matrix<Type, 3, 1> v1 = voxel_pos + Eigen::Matrix<Type, 3, 1>(0, voxel_size.y(), 0);
		Eigen::Matrix<Type, 3, 1> v2 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), 0, 0);
		Eigen::Matrix<Type, 3, 1> v3 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), 0, voxel_size.z());
		Eigen::Matrix<Type, 3, 1> v4 = v1 + Eigen::Matrix<Type, 3, 1>(0, 0, voxel_size.z());
		if (quad_intersection(ray_origin, ray_direction, v1, v2, v3, v4, hit) && face != face_in)
		{
			face_out = face;
			hit_out = hit;
			float half_voxel_y = voxel_size.y() * 0.5f;
			if (hit.y() > voxel_count.y() * voxel_size.y() - half_voxel_y)
				next_voxel_index = -1;
			else
				next_voxel_index = voxel_index + voxel_count.y();

			float dist = (hit_out - hit_in).norm();
			face_list.push_back(FaceData(face, hit, next_voxel_index, dist));
			//return true;
		}
	}

	face = BoxFace::Bottom;
	{
		Eigen::Matrix<Type, 3, 1> v1 = voxel_pos;
		Eigen::Matrix<Type, 3, 1> v2 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), 0, 0);
		Eigen::Matrix<Type, 3, 1> v3 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), 0, voxel_size.z());
		Eigen::Matrix<Type, 3, 1> v4 = v1 + Eigen::Matrix<Type, 3, 1>(0, 0, voxel_size.z());
		if (quad_intersection(ray_origin, ray_direction, v1, v2, v3, v4, hit) && face != face_in)
		{
			face_out = face;
			hit_out = hit;
			float half_voxel_y = voxel_size.y() * 0.5f;
			if (hit.y() < half_voxel_y)
				next_voxel_index = -1;
			else
				next_voxel_index = voxel_index - voxel_count.y();

			float dist = (hit_out - hit_in).norm();
			face_list.push_back(FaceData(face, hit, next_voxel_index, dist));
			//return true;
		}
	}

	face = BoxFace::Front;
	{
		Eigen::Matrix<Type, 3, 1> v1 = voxel_pos;
		Eigen::Matrix<Type, 3, 1> v2 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), 0, 0);
		Eigen::Matrix<Type, 3, 1> v3 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), voxel_size.y(), 0);
		Eigen::Matrix<Type, 3, 1> v4 = v1 + Eigen::Matrix<Type, 3, 1>(0, voxel_size.y(), 0);
		if (quad_intersection(ray_origin, ray_direction, v1, v2, v3, v4, hit) && face != face_in)
		{
			face_out = face;
			hit_out = hit;
			float half_voxel_z = voxel_size.z() * 0.5f;
			if (hit.z() < half_voxel_z)
				next_voxel_index = -1;
			else
				next_voxel_index = voxel_index - voxel_count.x() * voxel_count.y();

			float dist = (hit_out - hit_in).norm();
			face_list.push_back(FaceData(face, hit, next_voxel_index, dist));
			//return true;
		}
	}

	face = BoxFace::Rear;
	{
		Eigen::Matrix<Type, 3, 1> v1 = voxel_pos + Eigen::Matrix<Type, 3, 1>(0, 0, voxel_size.z());
		Eigen::Matrix<Type, 3, 1> v2 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), 0, 0);
		Eigen::Matrix<Type, 3, 1> v3 = v1 + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), voxel_size.y(), 0);
		Eigen::Matrix<Type, 3, 1> v4 = v1 + Eigen::Matrix<Type, 3, 1>(0, voxel_size.y(), 0);
		if (quad_intersection(ray_origin, ray_direction, v1, v2, v3, v4, hit) && face != face_in)
		{
			face_out = face;
			hit_out = hit;
			float half_voxel_z = voxel_size.z() * 0.5f;
			if (hit.z() > voxel_count.z() * voxel_size.z() - half_voxel_z)
				next_voxel_index = -1;
			else
				next_voxel_index = voxel_index + voxel_count.x()* voxel_count.y();

			float dist = (hit_out - hit_in).norm();
			face_list.push_back(FaceData(face, hit, next_voxel_index, dist));
			//return true;
		}
	}

	face = BoxFace::Left;
	{
		Eigen::Matrix<Type, 3, 1> v1 = voxel_pos;
		Eigen::Matrix<Type, 3, 1> v2 = v1 + Eigen::Matrix<Type, 3, 1>(0, 0, voxel_size.z());
		Eigen::Matrix<Type, 3, 1> v3 = v1 + Eigen::Matrix<Type, 3, 1>(0, voxel_size.y(), voxel_size.z());
		Eigen::Matrix<Type, 3, 1> v4 = v1 + Eigen::Matrix<Type, 3, 1>(0, voxel_size.y(), 0);
		if (quad_intersection(ray_origin, ray_direction, v1, v2, v3, v4, hit) && face != face_in)
		{
			face_out = face;
			hit_out = hit;
			float half_voxel_x = voxel_size.x() * 0.5f;
			if (hit.x() < half_voxel_x)
				next_voxel_index = -1;
			else
				next_voxel_index = voxel_index - 1;

			float dist = (hit_out - hit_in).norm();
			face_list.push_back(FaceData(face, hit, next_voxel_index, dist));
			//return true;
		}
	}

	face = BoxFace::Right;
	{
		Eigen::Matrix<Type, 3, 1> v1 = voxel_pos + Eigen::Matrix<Type, 3, 1>(voxel_size.x(), 0, 0);
		Eigen::Matrix<Type, 3, 1> v2 = v1 + Eigen::Matrix<Type, 3, 1>(0, 0, voxel_size.z());
		Eigen::Matrix<Type, 3, 1> v3 = v1 + Eigen::Matrix<Type, 3, 1>(0, voxel_size.y(), voxel_size.z());
		Eigen::Matrix<Type, 3, 1> v4 = v1 + Eigen::Matrix<Type, 3, 1>(0, voxel_size.y(), 0);
		if (quad_intersection(ray_origin, ray_direction, v1, v2, v3, v4, hit) && face != face_in)
		{
			face_out = face;
			hit_out = hit;
			float half_voxel_x = voxel_size.x() * 0.5f;
			if (hit.x() > voxel_count.x() * voxel_size.x() - half_voxel_x)
				next_voxel_index = -1;
			else
				next_voxel_index = voxel_index + 1;

			float dist = (hit_out - hit_in).norm();
			face_list.push_back(FaceData(face, hit, next_voxel_index, dist));
			//return true;
		}
	}

	if (face_list.size() > 1)
	{
		float max_dist = -1;
		for (auto d : face_list)
		{
			if (d.dist > max_dist)
			{
				face_out = d.face;
				hit_out = d.hit;
				next_voxel_index = d.voxel_index;
				max_dist = d.dist;
			}
		}
	}

	return (int)face_list.size();
}


template <typename Type>
BoxFace raycast_face_volume(
	const Eigen::Matrix<Type, 3, 1> ray_origin,
	const Eigen::Matrix<Type, 3, 1> ray_direction,
	const Eigen::Vector3i& voxel_count,
	const Eigen::Vector3i& voxel_size,
	int& voxel_index,
	Eigen::Matrix<Type, 3, 1>& hit,
	Eigen::Matrix<Type, 3, 1>& hit_normal)
{
	
	Eigen::Vector3i volume_size(
		voxel_count.x() * voxel_size.x(),
		voxel_count.y() * voxel_size.y(),
		voxel_count.z() * voxel_size.z());

	Eigen::Matrix<Type, 3, 1> half_volume_size = volume_size.cast<Type>() * (Type)0.5;
	int total_voxels = voxel_count.x() * voxel_count.y() * voxel_count.z();

	Eigen::Matrix<Type, 3, 1> half_voxel_size = voxel_size.cast<Type>() * (Type)0.5;

	Eigen::Matrix<Type, 3, 1> hit2;
	Eigen::Matrix<Type, 3, 1> hit2_normal;

	//
	// Check intersection with the whole volume
	//
	int intersections_count = box_intersection<Type>(
		ray_origin,
		ray_direction,
		half_volume_size,	//volume_center,
		//Eigen::Matrix<Type, 3, 1>::Zero(),
		volume_size.x(),
		volume_size.y(),
		volume_size.z(),
		hit,
		hit2,
		hit_normal,
		hit2_normal);

	if (intersections_count > 0)
	{
		voxel_index = get_index_from_3d(hit, voxel_count, voxel_size);
		return box_face_from_normal<float>(hit_normal);
	}
	else
	{
		voxel_index = -1;
		return BoxFace::Undefined;
	}

	
}




template <typename Type>
int raycast_face_in_out(
	const Eigen::Matrix<Type, 3, 1> ray_origin,
	const Eigen::Matrix<Type, 3, 1> ray_direction,
	const Eigen::Vector3i& voxel_count,
	const Eigen::Vector3i& voxel_size,
	BoxFace& face_in,
	BoxFace& face_out,
	Eigen::Matrix<Type, 3, 1>& hit_in,
	Eigen::Matrix<Type, 3, 1>& hit_out,
	Eigen::Matrix<Type, 3, 1>& hit_in_normal,
	Eigen::Matrix<Type, 3, 1>& hit_out_normal)
{

	Eigen::Vector3i volume_size(
		voxel_count.x() * voxel_size.x(),
		voxel_count.y() * voxel_size.y(),
		voxel_count.z() * voxel_size.z());

	Eigen::Matrix<Type, 3, 1> half_volume_size = volume_size.cast<Type>() * (Type)0.5;
	int total_voxels = voxel_count.x() * voxel_count.y() * voxel_count.z();

	Eigen::Matrix<Type, 3, 1> half_voxel_size = voxel_size.cast<Type>() * (Type)0.5;

	Eigen::Matrix<Type, 3, 1> hit1;
	Eigen::Matrix<Type, 3, 1> hit2;

	//
	// Check intersection with the whole volume
	//
	int intersections_count = box_intersection<Type>(
		ray_origin,
		ray_direction,
		half_volume_size,	//volume_center,
		//Eigen::Matrix<Type, 3, 1>::Zero(),
		volume_size.x(),
		volume_size.y(),
		volume_size.z(),
		hit1,
		hit2,
		hit_in_normal,
		hit_out_normal);

	if (intersections_count == 2)
	{
		face_in = box_face_from_normal<float>(hit_in_normal);
		face_out = box_face_from_normal<float>(hit_out_normal);

		hit_in = hit1;
		hit_out = hit2;
	}
	else if (intersections_count == 1)
	{
		face_in = face_out = box_face_from_normal<float>(hit_in_normal);
		hit_in = hit_out = hit1;
	}
	else
	{
		face_in = face_out = BoxFace::Undefined;
	}
	
	return intersections_count;

}



template <typename Type>
int raycast_volume(
	const Eigen::Matrix<Type, 3, 1> ray_origin,
	const Eigen::Matrix<Type, 3, 1> ray_direction,
	const Eigen::Vector3i& voxel_count,
	const Eigen::Vector3i& voxel_size,
	std::vector<int>& voxels_intersected)
{

	int voxel_index = -1;
	int next_voxel_index = -1;
	int intersections_count = 0;
	Eigen::Matrix<Type, 3, 1> hit_in;
	Eigen::Matrix<Type, 3, 1> hit_out;
	Eigen::Matrix<Type, 3, 1> hit_in_normal;
	Eigen::Matrix<Type, 3, 1> hit_out_normal;
	BoxFace face_in = BoxFace::Undefined;
	BoxFace face_out = BoxFace::Undefined;

	face_in = raycast_face_volume(ray_origin, ray_direction, voxel_count, voxel_size, voxel_index, hit_in, hit_in_normal);

	// the ray does not hits the volume
	if (face_in == BoxFace::Undefined || voxel_index < 0)
		return -1;

	intersections_count = raycast_face_in_out(ray_origin, ray_direction, voxel_count, voxel_size, face_in, face_out, hit_in, hit_out, hit_in_normal, hit_out_normal);

	bool is_inside = intersections_count > 0;


	while (is_inside)
	{
		voxels_intersected.push_back(voxel_index);

		if (face_intersections(ray_origin, ray_direction, voxel_count, voxel_size, voxel_index, face_in, hit_in, face_out, hit_out, next_voxel_index))
		{
#if 0
			std::cout << std::fixed
				<< "Face In   : " << box_face_to_string(face_in) << std::endl
				<< "Face Out  : " << box_face_to_string(face_out) << std::endl
				<< "Voxel In  : " << voxel_index << std::endl
				<< "Voxel Out : " << next_voxel_index << std::endl
				<< "Hit In    : " << hit_in.transpose() << std::endl
				<< "Hit Out   : " << hit_out.transpose() << std::endl
				<< std::endl;
#endif
			
			if (next_voxel_index < 0)
				is_inside = false;
			else
			{
				voxel_index = next_voxel_index;
				face_in = box_face_in_face_out(face_out);
				hit_in = hit_out;
			}
		}
		else
		{
			is_inside = false;
		}
	}

	return (int)voxels_intersected.size();
}



static bool has_same_sign_tsdf(const std::vector<Eigen::Vector2f>& voxels_params, int prev_voxel_index, int next_voxel_index)
{
	if (prev_voxel_index < 0 || prev_voxel_index > voxels_params.size() - 1 ||
		next_voxel_index < 0 || next_voxel_index > voxels_params.size() - 1)
		return false;

	return (voxels_params.at(prev_voxel_index).x() > 0 && voxels_params.at(next_voxel_index).x() > 0) ||
		(voxels_params.at(prev_voxel_index).x() < 0 && voxels_params.at(next_voxel_index).x() < 0);
}


template <typename Type>
int raycast_tsdf_volume(
	const Eigen::Matrix<Type, 3, 1> ray_origin,
	const Eigen::Matrix<Type, 3, 1> ray_direction,
	const Eigen::Vector3i& voxel_count,
	const Eigen::Vector3i& voxel_size,
	const std::vector<Eigen::Matrix<Type, 2, 1>>& params,
	Eigen::Matrix<Type, 3, 1>& hit_normal,
	long voxels_zero_crossing_indices[2])
{
	int voxel_index = -1;
	int next_voxel_index = -1;
	int intersections_count = 0;
	Eigen::Matrix<Type, 3, 1> hit_in;
	Eigen::Matrix<Type, 3, 1> hit_out;
	Eigen::Matrix<Type, 3, 1> hit_in_normal;
	Eigen::Matrix<Type, 3, 1> hit_out_normal;
	BoxFace face_in = BoxFace::Undefined;
	BoxFace face_out = BoxFace::Undefined;

	face_in = raycast_face_volume(ray_origin, ray_direction, voxel_count, voxel_size, voxel_index, hit_in, hit_in_normal);

	// the ray does not hits the volume
	if (face_in == BoxFace::Undefined || voxel_index < 0)
		return -1;

	intersections_count = raycast_face_in_out(ray_origin, ray_direction, voxel_count, voxel_size, face_in, face_out, hit_in, hit_out, hit_in_normal, hit_out_normal);

	bool is_inside = intersections_count > 0;

	while (is_inside)
	{
		if (face_intersections(ray_origin, ray_direction, voxel_count, voxel_size, voxel_index, face_in, hit_in, face_out, hit_out, next_voxel_index))
		{
			if (next_voxel_index < 0)
			{
				is_inside = false;
			}
			else
			{
				intersections_count++;

				face_in = box_face_in_face_out(face_out);
				hit_in = hit_out;

				if (!has_same_sign_tsdf(params, voxel_index, next_voxel_index))
				{
					voxels_zero_crossing_indices[0] = voxel_index;
					voxels_zero_crossing_indices[1] = next_voxel_index;
					voxel_index = next_voxel_index;
					intersections_count = 2;
					//hit_normal = hit_
					return intersections_count;
				}
				else
				{
					voxel_index = next_voxel_index;
				}
			}
		}
		else
		{
			is_inside = false;
		}
	}

	return intersections_count;
}


#endif	// _RAYCASTING_H_
