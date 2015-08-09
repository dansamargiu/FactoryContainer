/*
The MIT License (MIT)

Copyright (c) 2015 Dan Samargiu

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <unordered_map>
#include <memory>
#include <functional>
#include <typeindex>
#include <type_traits>
#include <vector>
#include <algorithm>

class FactoryContainer
{
private:
  typedef std::vector<std::type_index> type_list;
  typedef std::function<std::shared_ptr<void>(type_list*)> factory_value;

public:
  FactoryContainer() : m_factoryList()
  {
  }

  // Register Type I with Concrete Implementation T and dependencies
  // expanded from rest of template arguments.
  // Calls to Resolve will instantiate type T and recursively resolve
  // dependencies passing them into T's constructor as shared_ptrs
  // Internally calls Unregister on previous registrations for type I
  template <typename I, typename T, typename... Arguments>
  void RegisterType()
  {
    static_assert(std::is_base_of<I,T>::value,
      "RegisterType<I,T> called with unrelated types.");

    auto tFactory = [this](type_list * ancestor_list)
    {
      // sometimes we don't need the ancestor list as the argument package
      // is empty. this avoids unused parameter warning
      (void) ancestor_list;
      return std::make_shared<T>(this->Resolve<Arguments>(ancestor_list)...);
    };
    RegisterFactory<I>(std::move(tFactory));
  }

  // Register Type I with an instance defined by pInstance.
  // Calls to resolve will return the pointer to pInstance.
  // Internally calls Unregister on previous registrations for type I
  template <typename I>
  void RegisterInstance(std::shared_ptr<I> pInstance)
  {
    auto tFactory = [pInstance] (type_list*)
    {
      return pInstance;
    };
    RegisterFactory<I>(std::move(tFactory));
  }

  // Remove the associated type from the factory lookup
  // Calls to Resolve<I> will return nullptr after this call
  template <typename I>
  void Unregister()
  {
    auto key = std::type_index(typeid(I));
    auto it = m_factoryList.find(key);
    if (it != m_factoryList.end())
    {
      m_factoryList.erase(it);
    }
  }

  // Resolve the registered type I
  // Returns a shared_ptr to object based on type or instance registrations
  template <typename I>
  std::shared_ptr<I> Resolve() const
  {
    // ancestor list for dependency loop detection
    type_list ancestor_list;
    return Resolve<I>(&ancestor_list);
  }

private:
  std::unordered_map<std::type_index, factory_value> m_factoryList;

  template <typename I>
  void RegisterFactory(factory_value&& tFactory)
  {
    // Always unregister so we don't end up with duplicate entries.
    Unregister<I>();

    // Push the factory onto our list.
    auto key = std::type_index(typeid(I));
    m_factoryList[key] = std::move(tFactory);
  }

  template <typename I>
  std::shared_ptr<I> Resolve(type_list* ancestor_list) const
  {
    auto key = std::type_index(typeid(I));
    auto it = m_factoryList.find(key);
    if (it == m_factoryList.end())
    {
      return nullptr;
    }

    // If this type is already in the ancestor list, return nullptr
    // to prevent circular dependency loop
    auto found = std::find(ancestor_list->begin(), ancestor_list->end(), key);
    if ( found != ancestor_list->end())
    {
      return nullptr;
    }

    // Depth first traversal, push key onto the list
    // so we can detect circular dependencies
    ancestor_list->push_back(key);
    auto pObj = it->second(ancestor_list);
    ancestor_list->pop_back();

    return std::static_pointer_cast<I>(pObj);
  }

  // Disable Copy and Assign.
  FactoryContainer(FactoryContainer&) = delete;
  void operator=(FactoryContainer) = delete;
};
