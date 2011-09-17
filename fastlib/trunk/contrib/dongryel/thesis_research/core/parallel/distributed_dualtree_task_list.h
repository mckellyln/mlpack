/** @file distributed_dualtree_task_list.h
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef CORE_PARALLEL_DISTRIBUTED_DUALTREE_TASK_LIST_H
#define CORE_PARALLEL_DISTRIBUTED_DUALTREE_TASK_LIST_H

#include <boost/intrusive_ptr.hpp>
#include <boost/serialization/serialization.hpp>
#include <map>
#include <vector>
#include "core/parallel/distributed_dualtree_task.h"
#include "core/parallel/distributed_dualtree_task_queue.h"
#include "core/table/sub_table.h"

namespace core {
namespace parallel {

template < typename DistributedTableType,
         typename TaskPriorityQueueType >
class DistributedDualtreeTaskQueue;

template < typename DistributedTableType,
         typename TaskPriorityQueueType >
class DistributedDualtreeTaskList {

  public:

    typedef typename TaskPriorityQueueType::value_type TaskType;

    typedef typename TaskType::TableType TableType;

    typedef core::table::SubTable<TableType> SubTableType;

    typedef typename TableType::TreeType TreeType;

    typedef typename SubTableType::SubTableIDType KeyType;

    typedef core::parallel::DistributedDualtreeTaskQueue <
    DistributedTableType,
    TaskPriorityQueueType > DistributedDualtreeTaskQueueType;

    typedef int ValueType;

    struct ComparatorType {
      bool operator()(const KeyType &k1, const KeyType &k2) const {
        return k1.get<0>() < k2.get<0>() ||
               (k1.get<0>() == k2.get<0>() && k1.get<1>() < k2.get<1>()) ||
               (k1.get<0>() == k2.get<0>() && k1.get<1>() == k2.get<1>() &&
                k1.get<2>() < k2.get<2>()) ;
      }
    };

    typedef std::map< KeyType, ValueType, ComparatorType > MapType;

  private:

    /** @brief The destination MPI rank for which tasks are to be
     *         donated.
     */
    int destination_rank_;

    /** @brief The pointer to the distributed task queue.
     */
    DistributedDualtreeTaskQueueType *distributed_task_queue_;

    /** @brief The donated task list of (Q, R) pairs. Indexes the
     *         subtable positions.
     */
    std::vector< std::pair<int, std::vector<int> > > donated_task_list_;

    /** @brief The map that maps the subtable ID to the position.
     */
    MapType id_to_position_map_;

    /** @brief Denotes the number of extra points that can be
     *         transferred.
     */
    unsigned long int remaining_extra_points_to_hold_;

    /** @brief The subtables that must be transferred. The second
     *         component tells whether the subtable is referenced as a
     *         query. The third pair denotes the number of times the
     *         subtable is referenced as a reference set.
     */
    std::vector <
    boost::tuple <
    boost::intrusive_ptr<SubTableType> , bool, int > > sub_tables_;

    /** @brief The communicator for exchanging information.
     */
    boost::mpi::communicator *world_;

  private:

    void Print_() const {

      // Print out the mapping.
      typename MapType::const_iterator it = id_to_position_map_.begin();
      printf("Mapping from subtable IDs to position:\n");
      for(; it != id_to_position_map_.end(); it++) {
        printf(
          "(%d %d %d) -> %d\n",
          it->first.get<0>(), it->first.get<1>(), it->first.get<2>(),
          it->second);
      }

      // Print out the subtables.
      for(unsigned int i = 0; i < sub_tables_.size(); i++) {
        KeyType subtable_id = sub_tables_[i].get<0>()->subtable_id();
        printf("%d: (%d %d %d)\n", i, subtable_id.get<0>(),
               subtable_id.get<1>(), subtable_id.get<2>());
      }
    }

    /** @brief Returns the position of the subtable.
     */
    bool FindSubTable_(const KeyType &subtable_id, int *position_out) {
      typename MapType::iterator it =
        id_to_position_map_.find(subtable_id);
      printf("Finding %d %d %d\n", subtable_id.get<0>(),
             subtable_id.get<1>(), subtable_id.get<2>());
      if(it != id_to_position_map_.end()) {
        *position_out = it->second;
        printf("Found %d\n", it->second);
        return true;
      }
      else {
        *position_out = -1;
        printf("Not found!\n");
        return false;
      }
    }

    /** @brief Removes the subtable with the given ID.
     */
    void pop_(const KeyType &subtable_id, bool count_as_query) {

      if(sub_tables_.size() > 0) {

        // Find the position in the subtable list.
        typename MapType::iterator remove_position_it =
          this->id_to_position_map_.find(subtable_id);

        // This position should be valid, if there is at least one
        // subtable.
        int remove_position = remove_position_it->second;

        // Remove as a query table.
        if(count_as_query) {
          sub_tables_[remove_position].get<1>() = false;
        }

        // Otherwise, remove as a reference table by decrementing the
        // reference count.
        else {
          sub_tables_[remove_position].get<2>()--;
        }

        // If the subtable is no longer is referenced, we have to
        // remove it.
        if((! sub_tables_[remove_position].get<1>()) &&
            sub_tables_[remove_position].get<2>() == 0) {

          // Overwrite with the last subtable in the list and decrement.
          KeyType last_subtable_id = sub_tables_.back().get<0>()->subtable_id();
          remaining_extra_points_to_hold_ +=
            sub_tables_[remove_position].get<0>()->start_node()->count();
          id_to_position_map_.erase(
            sub_tables_[remove_position].get<0>()->subtable_id());
          sub_tables_[ remove_position ].get<0>() =
            sub_tables_.back().get<0>();
          sub_tables_[ remove_position ].get<1>() =
            sub_tables_.back().get<1>();
          sub_tables_[ remove_position ].get<2>() =
            sub_tables_.back().get<2>();
          sub_tables_.pop_back();
          if(sub_tables_.size() > 0) {
            id_to_position_map_[ last_subtable_id ] = remove_position;
          }
          else {
            id_to_position_map_.erase(last_subtable_id);
          }
        }
      }
    }

    /** @brief Returns the assigned position of the subtable if it can
     *         be transferred within the limit.
     */
    int push_back_(SubTableType &test_subtable_in, bool count_as_query) {

      // If already pushed, then return.
      KeyType subtable_id = test_subtable_in.subtable_id();
      int existing_position;
      if(this->FindSubTable_(subtable_id, &existing_position)) {
        if(! count_as_query) {
          printf("Found %d %d %d as a reference table at %d.\n",
                 subtable_id.get<0>(), subtable_id.get<1>(),
                 subtable_id.get<2>(), existing_position);
          sub_tables_[ existing_position ].get<2>()++;
        }
        else {
          printf("Found %d %d %d as a query table at %d.\n",
                 subtable_id.get<0>(), subtable_id.get<1>(),
                 subtable_id.get<2>(), existing_position);
          sub_tables_[existing_position].get<0>()->Alias(test_subtable_in);
          sub_tables_[ existing_position ].get<1>() = true;
        }
        return existing_position;
      }

      // Otherwise, try to see whether it can be stored.
      else if(
        static_cast <
        unsigned long int >(test_subtable_in.start_node()->count()) <=
        remaining_extra_points_to_hold_) {
        sub_tables_.resize(sub_tables_.size() + 1);

        boost::intrusive_ptr< SubTableType > tmp_subtable(new SubTableType());
        sub_tables_.back().get<0>().swap(tmp_subtable);
        sub_tables_.back().get<0>()->Alias(test_subtable_in);
        if(count_as_query) {
          sub_tables_.back().get<1>() = true;
          sub_tables_.back().get<2>() = 0;
        }
        else {
          sub_tables_.back().get<1>() = false;
          sub_tables_.back().get<2>() = 1;
        }
        id_to_position_map_[subtable_id] = sub_tables_.size() - 1;

        printf("Fit in within %lu.\n", remaining_extra_points_to_hold_);
        remaining_extra_points_to_hold_ -=
          test_subtable_in.start_node()->count();
        return sub_tables_.size() - 1;
      }
      printf("Could not fit in within %lu.\n",
             remaining_extra_points_to_hold_);
      return -1;
    }

  public:

    /** @brief Returns the number of extra points.
     */
    unsigned long int remaining_extra_points_to_hold() const {
      return remaining_extra_points_to_hold_;
    }

    /** @brief The default constructor.
     */
    DistributedDualtreeTaskList() {
      destination_rank_ = 0;
      distributed_task_queue_ = NULL;
      remaining_extra_points_to_hold_ = 0;
      world_ = NULL;
    }

    /** @brief Exports the received task list to the distributed task
     *         queue.
     */
    template<typename MetricType>
    void Export(
      boost::mpi::communicator &world,
      const MetricType &metric_in, int source_rank_in,
      DistributedDualtreeTaskQueueType *distributed_task_queue_in) {

      // Set the queue pointer.
      distributed_task_queue_ = distributed_task_queue_in;

      // Get a free slot for each subtable.
      std::vector<int> assigned_cache_indices;
      for(unsigned int i = 0; i < sub_tables_.size(); i++) {
        KeyType subtable_id = sub_tables_[i].get<0>()->subtable_id();
        printf("Received %d %d %d: %d %d, %d %d\n", subtable_id.get<0>(),
               subtable_id.get<1>(), subtable_id.get<2>(),
               sub_tables_[i].get<1>(), sub_tables_[i].get<2>(),
               sub_tables_[i].get<0>()->is_alias(),
               sub_tables_[i].get<0>()->is_query_subtable());

        assigned_cache_indices.push_back(
          distributed_task_queue_->push_subtable(
            *(sub_tables_[i].get<0>()), sub_tables_[i].get<2>()));
        printf("Assigned the cache block id: %d, checking %d\n",
               assigned_cache_indices[i],
               sub_tables_[i].get<0>()->is_alias());
      }

      // Now push in the task list for each query subtable.
      for(unsigned int i = 0; i < donated_task_list_.size(); i++) {
        int query_subtable_position = donated_task_list_[i].first;
        SubTableType *query_subtable_in_cache =
          distributed_task_queue_->FindSubTable(
            assigned_cache_indices[query_subtable_position]);
        int new_position =
          distributed_task_queue_->PushNewQueue(
            source_rank_in, *query_subtable_in_cache);
        for(unsigned int j = 0; j < donated_task_list_[i].second.size(); j++) {
          int reference_subtable_position = donated_task_list_[i].second[j];
          SubTableType *reference_subtable_in_cache =
            distributed_task_queue_->FindSubTable(
              assigned_cache_indices[ reference_subtable_position ]);
          distributed_task_queue_->PushTask(
            world, metric_in, new_position, *reference_subtable_in_cache);
        }
      }
    }

    /** @brief Initializes the task list.
     */
    void Init(
      boost::mpi::communicator &world,
      int destination_rank_in,
      unsigned long int remaining_extra_points_to_hold_in,
      DistributedDualtreeTaskQueueType &distributed_task_queue_in) {
      destination_rank_ = destination_rank_in;
      distributed_task_queue_ = &distributed_task_queue_in;
      remaining_extra_points_to_hold_ = remaining_extra_points_to_hold_in;
      world_ = &world;
    }

    /** @brief Tries to fit in as many tasks from the given query
     *         subtree.
     *
     *  @return true if the query subtable is successfully locked.
     */
    bool push_back(boost::mpi::communicator &world, int probe_index) {

      // First, we need to serialize the query subtree.
      SubTableType &query_subtable =
        distributed_task_queue_->query_subtable(probe_index);
      int query_subtable_position;
      if((query_subtable_position =
            this->push_back_(query_subtable, true)) < 0) {
        printf("Could not push in the query subtable within the limit.\n\n");
        return false;
      }
      donated_task_list_.resize(donated_task_list_.size() + 1);
      donated_task_list_.back().first = query_subtable_position;

      // And its associated reference sets.
      while(distributed_task_queue_->size(probe_index) > 0) {
        TaskType &test_task =
          const_cast<TaskType &>(distributed_task_queue_->top(probe_index));
        unsigned long int stolen_local_computation = test_task.work();
        int reference_subtable_position;

        // If the reference subtable cannot be packed, break.
        if((reference_subtable_position =
              this->push_back_(test_task.reference_subtable(), false)) < 0) {
          break;
        }
        else {

          // Pop from the list. Releasing each reference subtable from
          // the cache is done in serialization.
          distributed_task_queue_->pop(probe_index);
          donated_task_list_.back().second.push_back(
            reference_subtable_position);

          // For each reference subtree stolen, the amount of local
          // computation decreases.
          distributed_task_queue_->decrement_remaining_local_computation(
            stolen_local_computation);
        }
      } // end of trying to empty out a query subtable list.

      // If no reference subtable was pushed in, there is no point in
      // sending the query subtable.
      if(donated_task_list_.back().second.size() == 0) {
        this->pop_(query_subtable.subtable_id(), true);
        printf("Popping back the query subtable... because the reference");
        printf(" could not fit in.\n\n");
        donated_task_list_.pop_back();
        return false;
      }
      else {

        printf("Fit in! %d %d %d\n\n", query_subtable.subtable_id().get<0>(),
               query_subtable.subtable_id().get<1>(),
               query_subtable.subtable_id().get<2>());
        // Otherwise, lock the query subtable.
        distributed_task_queue_->LockQuerySubTable(
          probe_index, destination_rank_);
        return true;
      }
    }

    void ReleaseCache() {
      for(unsigned int i = 0; i < sub_tables_.size(); i++) {
        // If this is a reference subtable, then we need to release
        // it from the cache owned by the donating process.
        const_cast <
        DistributedDualtreeTaskQueueType * >(
          distributed_task_queue_)->ReleaseCache(
            *world_,
            sub_tables_[i].get<0>()->cache_block_id(),
            sub_tables_[i].get<2>());
      }
    }

    /** @brief Saves the donated task list.
     */
    template<class Archive>
    void save(Archive &ar, const unsigned int version) const {

      this->Print_();

      // Save the number of subtables transferred.
      int num_subtables = sub_tables_.size();
      ar & num_subtables;
      printf("Saving %d subtables.\n", num_subtables);
      if(num_subtables > 0) {
        for(int i = 0; i < num_subtables; i++) {
          ar & (*(sub_tables_[i].get<0>()));
          ar & sub_tables_[i].get<2>();
          printf("  %d %d %d %d %d\n",
                 (sub_tables_[i].get<0>())->subtable_id().get<0>(),
                 (sub_tables_[i].get<0>())->subtable_id().get<1>(),
                 (sub_tables_[i].get<0>())->subtable_id().get<2>(),
                 sub_tables_[i].get<0>()->is_query_subtable(),
                 sub_tables_[i].get<2>());
        }

        // Save the donated task lists.
        int num_donated_lists = donated_task_list_.size();
        ar & num_donated_lists;
        for(int i = 0; i < num_donated_lists; i++) {
          int sublist_size = donated_task_list_[i].second.size();
          ar & donated_task_list_[i].first;
          ar & sublist_size;
          for(int j = 0; j < sublist_size; j++) {
            ar & donated_task_list_[i].second[j];
          }
        }
      }
    }

    /** @brief Loads the donated task list.
     */
    template<class Archive>
    void load(Archive &ar, const unsigned int version) {

      // Load the number of subtables transferred.
      int num_subtables;
      ar & num_subtables;
      printf("Loading %d subtables.\n", num_subtables);
      if(num_subtables > 0) {
        sub_tables_.resize(num_subtables);
        for(int i = 0; i < num_subtables; i++) {

          // Need to the cache block correction later.
          sub_tables_[i].get<0>() = new SubTableType();
          sub_tables_[i].get<0>()->Init(i, false);
          ar & (*(sub_tables_[i].get<0>()));
          ar & sub_tables_[i].get<2>();
        }

        // Load the donated task lists.
        int num_donated_lists;
        ar & num_donated_lists;
        donated_task_list_.resize(num_donated_lists);
        for(int i = 0; i < num_donated_lists; i++) {
          int sublist_size;
          ar & donated_task_list_[i].first;
          ar & sublist_size;
          donated_task_list_[i].second.resize(sublist_size);
          for(int j = 0; j < sublist_size; j++) {
            ar & donated_task_list_[i].second[j];
          }
        }
      }
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};
}
}

#endif
