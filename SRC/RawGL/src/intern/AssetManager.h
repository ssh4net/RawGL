#pragma once

#include "Common.h"
/*
template<class T>
class AssetDeleter
{
public:
	//AssetDeleter() {}
	AssetDeleter(std::map<std::string, std::weak_ptr<T>>* p) :
		m_listPtr(p)
	{}

	void operator()(T* p)
	{
		//LOG(debug) << "AssetDeleter()";
		std::cout << "AssetDeleter()" << std::endl;
		m_listPtr->erase(*p);
		std::default_delete<T>()(p);
	}

private:
	std::map<std::string, std::weak_ptr<T>>* m_listPtr;
};
*/
template<class T>
class AssetManager
{
public:
	// Check for existence, but don't allocate
	std::shared_ptr<T> find(const std::string& name)
	{
		auto it = m_list.find(name);

		if (it != m_list.end())
			return it->second;

		return std::shared_ptr<T>();
	}
	
protected:
	std::map<std::string, std::shared_ptr<T>> m_list;

	// Construct an object & add to the list
	/*
	std::map<std::string, std::weak_ptr<T>> m_list;

	template<typename ... V>
	std::map<std::string, std::weak_ptr<T>>::iterator add(V ... values)
	{
		//std::map<std::string, std::weak_ptr<T>>::iterator

		//LOG(debug) << "AssetManager alloc";
		std::cout << "AssetManager alloc" << std::endl;
		//return std::shared_ptr<T>(new T(values ...), AssetDeleter<T>());
		return std::shared_ptr<T>(
			new T(values ...),
			AssetDeleter<T>(
				&m_list,
				it)
			);
	}
	*/
};
