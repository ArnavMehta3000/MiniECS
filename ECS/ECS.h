#pragma once
#include <bitset>
#include <vector>
#include <type_traits>

namespace ECS
{
	constexpr size_t MAX_COMPONENTS = 64;
	constexpr size_t MAX_ENTITIES   = 1000000;

	using ComponentID   = unsigned long long;
	using EntityIndex   = unsigned int;
	using EntityVersion = unsigned int;
	using EntityID      = unsigned long long;  // Top 32 bits have index and bottom 32 bits have version
	using ComponentMask = std::bitset<MAX_COMPONENTS>;
	
	extern ComponentID s_componentCounter = 0;


	namespace  // Anon namespace for helper functions
	{
		inline EntityID CreateEntityId(EntityIndex index, EntityVersion version)
		{
			// Shift the index up 32, and put the version in the bottom
			return ((EntityID)index << 32) | ((EntityID)version);
		}

		inline EntityIndex GetEntityIndex(EntityID id)
		{
			// Shift down 32 so we lose the version and get our index
			return id >> 32;
		}

		inline EntityVersion GetEntityVersion(EntityID id)
		{
			// Cast to a 32 bit int to get our version number (loosing the top 32 bits)
			return (EntityVersion)id;
		}

		inline bool IsEntityValid(EntityID id)
		{
			// Check if the index is our invalid index
			return (id >> 32) != EntityIndex(-1);
		}
	}

#define INVALID_ENTITY ECS::CreateEntityId(EntityIndex(-1), 0)

	template <class T>
	ComponentID GetId()
	{
		static ComponentID s_componentId = s_componentCounter++;
		return s_componentId;
	}

	class ComponentPool
	{
	public:
		explicit ComponentPool(size_t elementSize)
			:
			m_elementSize(elementSize)
		{
			m_data = new std::byte[MAX_ENTITIES * m_elementSize];
		}

		~ComponentPool()
		{
			delete[] m_data;
		}
		
		ComponentPool() = delete;
		bool operator==(const ComponentPool& other) const = delete;

		template <typename T>
		T* Get(size_t index)
		{
			return reinterpret_cast<T*>(&m_data[index * m_elementSize]);
		}

	private:
		size_t m_elementSize{ 0 };
		std::byte* m_data{ nullptr };
	};

	class World
	{
	private:
		// Contains the entity's id and component mask
		struct EntityDesc
		{
			EntityID m_id;
			ComponentMask m_mask;
		};

	public:
		World() = default;

		[[maybe_unused]] 
		EntityID NewEntity()
		{
			// Check for free slots
			if (!m_freeEntities.empty())
			{
				EntityIndex newIndex = m_freeEntities.back();
				m_freeEntities.pop_back();
				
				EntityID newID = CreateEntityId(newIndex, GetEntityVersion(m_entities[newIndex].m_id));
				m_entities[newIndex].m_id = newID;
				
				return m_entities[newIndex].m_id;
			}

			m_entities.push_back(
				{  // Entity desc
					CreateEntityId(EntityIndex(m_entities.size()), 0),
					ComponentMask()
				});

			return m_entities.back().m_id;
		}

		void DestroyEntity(EntityID id)
		{
			EntityID newID = CreateEntityId(EntityIndex(-1), GetEntityVersion(id) + 1);
			m_entities[GetEntityIndex(id)].m_id = newID;
			m_entities[GetEntityIndex(id)].m_mask.reset();
			m_freeEntities.push_back(GetEntityIndex(GetEntityIndex(id)));
		}
		
		template <typename T, typename... Args>
		T*  Assign(EntityID id, Args&&... args) requires
			std::is_constructible_v<T>
			&& (std::is_constructible_v<T, Args...> || std::is_default_constructible_v<T>)
		{
			ComponentID componentId = GetId<T>();

			// Ensures you're not accessing an entity that has been deleted
			if (m_entities[GetEntityIndex(id)].m_id != id)
				return nullptr;

			// Resize the component pool vector if necessary
			if (componentId >= m_componentPools.size())
				m_componentPools.resize(componentId + 1, nullptr);
			
			if (m_componentPools[componentId] == nullptr)  // New component, make a new pool
				m_componentPools[componentId] = new ComponentPool(sizeof(T));

			// Looks up the component in the pool and initializes it with placement new
			if constexpr (std::is_constructible_v<T, Args...>) 
			{
				T* comp = new (m_componentPools[componentId]->Get<T>(GetEntityIndex(id))) T(std::forward<Args>(args)...);
				m_entities[GetEntityIndex(id)].m_mask.set(componentId);
				return comp;
			}
			else if constexpr (std::is_default_constructible_v<T>) 
			{
				T* comp = new (m_componentPools[componentId]->Get<T>(GetEntityIndex(id))) T();
				m_entities[GetEntityIndex(id)].m_mask.set(componentId);
				return comp;
			}
			else
				return nullptr; // Component type cannot be constructed

		}

		template<typename T>
		void Remove(EntityID id)
		{
			// Ensures you're not accessing an entity that has been deleted
			if (m_entities[GetEntityIndex(id)].m_id != id)
				return;

			ComponentID componentId = GetId<T>();
			m_entities[GetEntityIndex(id)].m_mask.reset(componentId);
		}

		template <typename T>
		[[nodiscard]] 
		T* Get(EntityID id)
		{
			ComponentID componentId = GetId<T>();
			if (!m_entities[GetEntityIndex(id)].m_mask.test(componentId))
				return nullptr;

			if (componentId < m_componentPools.size() && m_componentPools[componentId])
				return m_componentPools[componentId]->Get<T>(GetEntityIndex(id));

			return nullptr;
		}

		template <typename T>
		bool Has(EntityID id)
		{
			return !(Get<T>(id) == nullptr);
		}


		std::vector<EntityDesc>& All()
		{
			return m_entities;
		}

	private:		
		std::vector<EntityDesc>     m_entities;        // List of all the entities in a m_rosterPtr
		std::vector<EntityIndex>    m_freeEntities;    // List of all free entity indices
		std::vector<ComponentPool*> m_componentPools;  // List of component pools
	};

	template <typename... ComponentTypes>
	class RosterView
	{
	public:
		RosterView(World& roster)
			:
			m_rosterPtr(&roster)
		{
			if (sizeof...(ComponentTypes) == 0)
			{
				m_all = true;
			}
			else
			{
				// Unpack the template parameters into an initializer list
				ComponentID componentIds[] = { 0, GetId<ComponentTypes>() ... };

				for (ComponentID componentId : componentIds)
				{
					m_componentMask.set(componentId);
				}
			}
		}

		EntityID operator*() const
		{
			return m_rosterPtr->All()[m_index].m_id;
		}

		bool operator==(const RosterView& other) const
		{
			return m_index == other.m_index || m_index == m_rosterPtr->All().size();
		}

		bool operator!=(const RosterView& other) const
		{
			return m_index != other.m_index && m_index != m_rosterPtr->All().size();
		}

		RosterView& operator++()
		{
			// Keep going next until valid mask is found
			do
			{
				m_index++;
			} while (m_index < m_rosterPtr->All().size() && !ValidIndex());

			return *this;
		}

		RosterView begin()
		{
			int firstIndex = 0;
			while (
				firstIndex < m_rosterPtr->All().size()
				&& (m_componentMask != (m_componentMask & m_rosterPtr->All()[firstIndex].m_mask)
					|| !IsEntityValid(m_rosterPtr->All()[firstIndex].m_id)))
			{
				firstIndex++;
			}

			return RosterView(*m_rosterPtr, firstIndex, m_componentMask);
		}

		RosterView end()
		{
			return RosterView(*m_rosterPtr, EntityIndex(m_rosterPtr->All().size()), m_componentMask);
		}

	private:
		RosterView(World& roster, EntityIndex index, ComponentMask mask) 
			: 
			RosterView(roster)
		{
			m_index = index;
			m_componentMask = mask;
		}

		bool ValidIndex()
		{
			bool validEntity = IsEntityValid(m_rosterPtr->All()[m_index].m_id);
			bool validComponentMask = (m_all || m_componentMask == (m_componentMask & m_rosterPtr->All()[m_index].m_mask));

			return validEntity && validComponentMask;
		}

	private:
		EntityIndex   m_index{ 0 };
		World*       m_rosterPtr{ nullptr };
		ComponentMask m_componentMask;
		bool          m_all{ false };
	};
}
