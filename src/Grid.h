#ifndef __GRID_H__
#define __GRID_H__

#include <Eigen/Dense>
#include <vector>
#include <iostream>
#include "RayBox.h"
#include "RayIntersection.h"


template<typename Type>
struct Voxel
{
	int index;
	Eigen::Matrix<Type, 3, 1> point;
	Eigen::Matrix<Type, 3, 1> rgb;
	Type tsdf;
	Type tsdf_raw;
	Type sdf;
	Type weight;

	Voxel() :index(0), tsdf(-1), tsdf_raw(-1), sdf(-1), weight(1.0){}
};
typedef Voxel<double> Voxeld;
typedef Voxel<float> Voxelf;


template<typename Type>
class Grid
{

public:

	Grid(const Eigen::Matrix<Type, 3, 1>& _volume_size, const Eigen::Matrix<Type, 3, 1>& _voxel_size, const Eigen::Matrix<Type, 4, 4>& _transformation)
	{
		this->reset(_volume_size, _voxel_size, _transformation);
	}
	
	~Grid() 
	{ 
	}


	void reset(const Eigen::Matrix<Type, 3, 1>& _volume_size, const Eigen::Matrix<Type, 3, 1>& _voxel_size, const Eigen::Matrix<Type, 4, 4>& _transformation)
	{
		this->transformation = _transformation;
		this->volume_size = _volume_size;
		this->voxel_size = _voxel_size;
		this->voxel_count = Eigen::Vector3i(volume_size.x() / voxel_size.x(), volume_size.y() / voxel_size.y(), volume_size.z() / voxel_size.z());
		this->data.resize((voxel_count.x() + 1) * (voxel_count.y() + 1) * (voxel_count.z() + 1));

		Eigen::Matrix<Type, 4, 4> to_origin = Eigen::Matrix<Type, 4, 4>::Identity();
		to_origin.col(3) << -(volume_size.x() / 2.0), -(volume_size.y() / 2.0), -(volume_size.z() / 2.0), 1.0;	// set translate

		int i = 0;
		for (int z = 0; z <= volume_size.z(); z += voxel_size.z())
		{
			for (int y = 0; y <= volume_size.y(); y += voxel_size.y())
			{
				for (int x = 0; x <= volume_size.x(); x += voxel_size.x(), i++)
				{
					Eigen::Vector4d p = _transformation * to_origin * Eigen::Vector4d(x, y, z, 1);
					p /= p.w();

					data[i].point = p.head<3>();
					data[i].rgb = Eigen::Matrix<Type, 3, 1>(0, 0, 0);
					data[i].weight = 1.0;
					data[i].index = i;
					data[i].tsdf = 0;
					data[i].tsdf_raw = 0;
					data[i].sdf = 0;
				}
			}
		}
	}

	int index_array_from_3d_index(Eigen::Vector3i pt) const
	{
		return (pt.z() * (volume_size.x() + 1) * (volume_size.y() + 1)) + (pt.y() * (volume_size.y() + 1)) + pt.x();
	}

	Eigen::Vector3i index_3d_from_array_index(int array_index) const
	{
		return index_3d_from_array_index(array_index, this->volume_size, this->voxel_size);
	}

	static Eigen::Vector3i index_3d_from_array_index(int array_index, const Eigen::Matrix<Type, 3, 1>& _volume_size, const Eigen::Matrix<Type, 3, 1>& _voxel_size)
	{
		return Eigen::Vector3i(
			int(std::fmod(array_index, _volume_size.x() + 1)),
			int(std::fmod(array_index / (_volume_size.y() + 1), (_volume_size.y() + 1))),
			int(array_index / ((_volume_size.x() + 1) * (_volume_size.y() + 1))));
	}


	std::vector<int> neighbour_eight(int voxel_index) const
	{
		return neighbour_eight(voxel_index, this->volume_size, this->voxel_size);
	}


	static std::vector<int> neighbour_eight(int voxel_index, const Eigen::Matrix<Type, 3, 1>& _volume_size, const Eigen::Matrix<Type, 3, 1>& _voxel_size)
	{
		std::vector<int> neighbours;

		auto index_3d = index_3d_from_array_index(voxel_index, _volume_size, _voxel_size);
		
		Type x = index_3d.x();
		Type y = index_3d.y();
		Type z = index_3d.z();

		if (index_3d.x() > 0 && index_3d.y() > 0)									// x - 1 , y - 1
			neighbours.push_back(voxel_index - _volume_size.x() - 2);
				
		if (index_3d.x() > 0)														// x - 1 , y
			neighbours.push_back(voxel_index - 1);
				
		if (index_3d.x() > 0 && index_3d.y() < _volume_size.y())					// x - 1 , y + 1
			neighbours.push_back(voxel_index + _volume_size.x());
				
		if (index_3d.y() < _volume_size.y())										// x , y + 1
			neighbours.push_back(voxel_index + _volume_size.x() + 1);
				
		if (index_3d.x() < _volume_size.x() && index_3d.y() < _volume_size.y())		// x + 1 , y + 1
			neighbours.push_back(voxel_index + _volume_size.x() + 2);
				
		if (index_3d.x() < _volume_size.x())										// x + 1 , y
			neighbours.push_back(voxel_index + 1);

		if (index_3d.x() < _volume_size.x() && index_3d.y() > 0)					// x + 1 , y - 1
			neighbours.push_back(voxel_index - _volume_size.x());

		if (index_3d.y() > 0)														// x , y - 1
			neighbours.push_back(voxel_index - _volume_size.x() - 1);

		return neighbours;
	}


	std::vector<int> find_intersections_in_neighbours(
		const int voxel_index,
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far) const
	{
		std::vector<int>& neighbours = neighbour_eight(voxel_index);
		//neighbours.push_back(voxel_index);
		//return find_intersections(neighbours, data, volume_size, voxel_size, transformation, origin, direction, ray_near, ray_far);
		return find_intersections(neighbours, data, volume_size, voxel_size, Eigen::Matrix<Type, 4, 4>::Identity(), origin, direction, ray_near, ray_far);
	}

	std::vector<int> find_intersections_in_neighbours(
		const std::vector<int>& neighbours,
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far) const
	{
		return find_intersections(neighbours, data, volume_size, voxel_size, Eigen::Matrix<Type, 4, 4>::Identity(), origin, direction, ray_near, ray_far);
	}


	static std::vector<int> find_intersections(
		const std::vector<int>& voxel_indices,
		const std::vector<Voxel<Type>>& volume,
		const Eigen::Matrix<Type, 3, 1>& volume_size,
		const Eigen::Matrix<Type, 3, 1>& voxel_size,
		const Eigen::Matrix<Type, 4, 4>& volume_transformation,
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far)
	{
		Eigen::Matrix<Type, 3, 1> half_voxel(voxel_size.x() * 0.5, voxel_size.y() * 0.5, voxel_size.z() * 0.5);

		std::vector<int> intersections;

		for (const int i : voxel_indices)
		{
			const Voxel<Type>& v = volume[i];
			Eigen::Matrix<Type, 3, 1> corner_min = (volume_transformation * (v.point - half_voxel).homogeneous()).head<3>();
			Eigen::Matrix<Type, 3, 1> corner_max = (volume_transformation * (v.point + half_voxel).homogeneous()).head<3>();

			Box<Type> box(corner_min, corner_max);
			Ray<Type> ray(origin, direction);

			if (box.intersect(ray, ray_near, ray_far))
			{
				intersections.push_back(i);
			}
		}

		return intersections;
	}

	static std::vector<int> find_intersections(
		const std::vector<Voxel<Type>>& volume,
		const Eigen::Matrix<Type, 3, 1>& volume_size,
		const Eigen::Matrix<Type, 3, 1>& voxel_size,
		const Eigen::Matrix<Type, 4, 4>& volume_transformation,
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far)
	{
		Eigen::Matrix<Type, 3, 1> half_voxel(voxel_size.x() * 0.5, voxel_size.y() * 0.5, voxel_size.z() * 0.5);

		std::vector<int> intersections;

		int i = 0;
		for (const Voxel<Type>& v : volume)
		{
			Eigen::Matrix<Type, 3, 1> corner_min = (volume_transformation * (v.point - half_voxel).homogeneous()).head<3>();
			Eigen::Matrix<Type, 3, 1> corner_max = (volume_transformation * (v.point + half_voxel).homogeneous()).head<3>();

			Box<Type> box(corner_min, corner_max);
			Ray<Type> ray(origin, direction);

			if (box.intersect(ray, ray_near, ray_far))
			{
				intersections.push_back(i);
			}
			++i;
		}

		return intersections;
	}


	std::vector<int> raycast_all(
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far) const
	{
		Eigen::Matrix<Type, 3, 1> half_voxel(voxel_size.x() * 0.5, voxel_size.y() * 0.5, voxel_size.z() * 0.5);

		std::vector<int> intersections;

		int voxel_index = 0;
		for (const Voxel<Type> v : data)
		{
			//Eigen::Matrix<Type, 3, 1> corner_min = (transformation * (v.point - half_voxel).homogeneous()).head<3>();
			//Eigen::Matrix<Type, 3, 1> corner_max = (transformation * (v.point + half_voxel).homogeneous()).head<3>();
			Eigen::Matrix<Type, 3, 1> corner_min = (v.point - half_voxel).homogeneous().head<3>();
			Eigen::Matrix<Type, 3, 1> corner_max = (v.point + half_voxel).homogeneous().head<3>();

			Box<Type> box(corner_min, corner_max);
			Ray<Type> ray(origin, direction);
			

			if (box.intersect(ray, ray_near, ray_far))
				intersections.push_back(voxel_index);
			
			++voxel_index;
		}

		return intersections;
	}




	std::vector<int> raycast_all_ordered(
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far) const
	{

		std::vector<int> intersections = raycast_all(origin, direction, ray_near, ray_far);

		sort_intersections(intersections, data, origin);

		return intersections;
	}


	


	void recursive_raycast(
		int last_voxel_found,
		int voxel_index,
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far,
		std::vector<int>& intersections) const 
	{
		if (voxel_index < 0 || voxel_index >= data.size())
			return;

		if (intersect(voxel_index, origin, direction, ray_near, ray_far) && voxel_index != last_voxel_found)
		{
			if (intersections.size() > 0)
				last_voxel_found = intersections.back();
			intersections.push_back(voxel_index);
		}
		else
		{
			return;
		}

		const int left_voxel_index = voxel_index + 1;
		const int right_voxel_index = voxel_index - 1;
		const int up_voxel_index = voxel_index + volume_size.y() + 1;
		const int down_voxel_index = voxel_index - volume_size.y() - 1;
		const int front_voxel_index = voxel_index + ((volume_size.x() + 1) * (volume_size.y() + 1));
		const int back_voxel_index = voxel_index - ((volume_size.x() + 1) * (volume_size.y() + 1));

		recursive_raycast(last_voxel_found, left_voxel_index, origin, direction, ray_near, ray_far, intersections);
		recursive_raycast(last_voxel_found, right_voxel_index, origin, direction, ray_near, ray_far, intersections);
		recursive_raycast(last_voxel_found, up_voxel_index, origin, direction, ray_near, ray_far, intersections);
		recursive_raycast(last_voxel_found, down_voxel_index, origin, direction, ray_near, ray_far, intersections);
		recursive_raycast(last_voxel_found, front_voxel_index, origin, direction, ray_near, ray_far, intersections);
		recursive_raycast(last_voxel_found, back_voxel_index, origin, direction, ray_near, ray_far, intersections);
	}



	bool intersect(
		const int voxel_index,
		const Eigen::Matrix<Type, 3, 1>& origin,
		const Eigen::Matrix<Type, 3, 1>& direction,
		const Type ray_near,
		const Type ray_far) const
	{
		if (voxel_index < 0 || voxel_index > data.size() - 1)
			return false;

		Eigen::Matrix<Type, 3, 1> half_voxel(voxel_size.x() * 0.5, voxel_size.y() * 0.5, voxel_size.z() * 0.5);

		const Voxel<Type>& v = data[voxel_index];
		
		//Eigen::Matrix<Type, 3, 1> corner_min = (transformation * (v.point - half_voxel).homogeneous()).head<3>();
		//Eigen::Matrix<Type, 3, 1> corner_max = (transformation * (v.point + half_voxel).homogeneous()).head<3>();

		Eigen::Matrix<Type, 3, 1> corner_min = (v.point - half_voxel).homogeneous().head<3>();
		Eigen::Matrix<Type, 3, 1> corner_max = (v.point + half_voxel).homogeneous().head<3>();

		Box<Type> box(corner_min, corner_max);
		Ray<Type> ray(origin, direction);

		return box.intersect(ray, ray_near, ray_far);
	}


	static void sort_intersections(std::vector<int>& intersections, const std::vector<Voxel<Type>>& volume, const Eigen::Matrix<Type, 3, 1>& origin)
	{
		struct VoxelDistanceToOrigin
		{
			VoxelDistanceToOrigin(const std::vector<Voxel<Type>>* _volume_ptr, const Eigen::Matrix<Type, 3, 1>& _origin)
				: volume_ptr(_volume_ptr)
				, origin(_origin)
			{}

			const std::vector<Voxel<Type>>* volume_ptr;
			const Eigen::Matrix<Type, 3, 1> origin;

			bool operator()(const int& l, const int& r) const
			{
				const Type& distance_0 = (volume_ptr->at(l).point - origin).norm();
				const Type& distance_1 = (volume_ptr->at(r).point - origin).norm();
				return distance_0 < distance_1;
			}
		};

		VoxelDistanceToOrigin voxel_function(&volume, origin);
		std::sort(intersections.begin(), intersections.end(), voxel_function);
	}



	Eigen::Matrix<Type, 3, 1> volume_size;			// size of volume
	Eigen::Matrix<Type, 3, 1> voxel_size;				// size of voxels
	Eigen::Vector3i voxel_count;			// count of voxels
	std::vector<Voxel<Type>> data;				// array of voxels
	Eigen::Matrix<Type, 4, 4> transformation;			// volume transformation matrix
};




#endif // __GRID_H__