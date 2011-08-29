/** @file sub_table.h
 *
 *  An abstraction to serialize a part of a dataset and its associated
 *  subtree.
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef CORE_TABLE_SUB_TABLE_H
#define CORE_TABLE_SUB_TABLE_H

#include <vector>
#include <boost/serialization/serialization.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/utility.hpp>
#include "core/table/index_util.h"
#include "core/table/sub_dense_matrix.h"

namespace core {
namespace table {

extern MemoryMappedFile *global_m_file_;

/** @brief A subtable class for serializing/unserializing a part of a
 *         table object.
 */
template<typename IncomingTableType>
class SubTable {

  public:

    /** @brief The type of the table.
     */
    typedef IncomingTableType TableType;

    /** @brief The type of the tree.
     */
    typedef typename TableType::TreeType TreeType;

    /** @brief The type of the old from new indices.
     */
    typedef typename TableType::OldFromNewIndexType OldFromNewIndexType;

    /** @brief The type of the subtable.
     */
    typedef core::table::SubTable<TableType> SubTableType;

    /** @brief The ID type of the subtable.
     */
    typedef boost::tuple<int, int, int> SubTableIDType;

    /** @brief The class for indicating the node ID for which the
     *         points are serialized underneath.
     */
    class PointSerializeFlagType {
      private:

        // For boost serialization.
        friend class boost::serialization::access;

        /** @brief The begin index.
         */
        int begin_;

        /** @brief The count of the points.
         */
        int count_;

      public:

        /** @brief Returns the beginning index of the flag.
         */
        int begin() const {
          return begin_;
        }

        /** @brief Returns the count of the points.
         */
        int count() const {
          return count_;
        }

        /** @brief Returns the ending index of the flag.
         */
        int end() const {
          return begin_ + count_;
        }

        /** @brief Serialize/unserialize.
         */
        template<class Archive>
        void serialize(Archive &ar, const unsigned int version) {
          ar & begin_;
          ar & count_;
        }

        /** @brief The default constructor.
         */
        PointSerializeFlagType() {
          begin_ = 0;
          count_ = 0;
        }

        /** @brief Initialize with a begin/count pair.
         */
        PointSerializeFlagType(
          int begin_in, int count_in) {
          begin_ = begin_in;
          count_ = count_in;
        }
    };

  private:

    // For boost serialization.
    friend class boost::serialization::access;

    /** @brief Whether to serialize the new from old mapping.
     */
    bool serialize_new_from_old_mapping_;

    /** @brief The ID of the cache block the subtable is occupying.
     */
    int cache_block_id_;

    /** @brief The MPI rank of the process holding the write lock on
     *         this subtable.
     */
    int locked_mpi_rank_;

    /** @brief The table to be loaded/saved.
     */
    TableType *table_;

    /** @brief If not NULL, this points to the starting node whose
     *         subtree must be serialized.
     */
    TreeType *start_node_;

    /** @brief The pointer to the underlying data.
     */
    core::table::DenseMatrix *data_;

    /** @brief The pointer to the underlying weights.
     */
    core::table::DenseMatrix *weights_;

    /** @brief The pointer to the old_from_new mapping.
     */
    boost::interprocess::offset_ptr<OldFromNewIndexType> *old_from_new_;

    /** @brief The pointer to the new_from_old mapping.
     */
    boost::interprocess::offset_ptr<int> *new_from_old_;

    /** @brief The rank of the MPI process from which every query
     *         subtable/query result is derived. If not equal to the
     *         current MPI process rank, these must be written back
     *         when the task queue runs out.
     */
    int originating_rank_;

    /** @brief The pointer to the tree.
     */
    boost::interprocess::offset_ptr<TreeType> *tree_;

    /** @brief Whether the subtable is an alias of another subtable or
     *         not.
     */
    bool is_alias_;

    /** @brief The list each terminal node that is being
     *         serialized/unserialized, the beginning index and its
     *         boolean flag whether the points under it are all
     *         serialized or not.
     */
    std::vector< PointSerializeFlagType > serialize_points_per_terminal_node_;

  private:

    /** @brief Collects the tree nodes in a list form and marks
     *         whether each terminal node should have its points
     *         serialized or not.
     */
    void FillTreeNodes_(
      TreeType *node, int parent_node_index,
      std::vector< std::pair< TreeType *, int > > &sorted_nodes,
      std::vector <
      PointSerializeFlagType > *serialize_points_per_terminal_node_in,
      int level, bool add_serialize_points_per_terminal_node) const {

      if(node != NULL) {

        // Currently assumes that everything is serialized under the
        // start node.
        if(parent_node_index < 0 && add_serialize_points_per_terminal_node) {
          serialize_points_per_terminal_node_in->push_back(
            PointSerializeFlagType(node->begin(), node->count()));
        }

        sorted_nodes.push_back(
          std::pair<TreeType *, int>(node, parent_node_index));

        // If the node is not a leaf,
        if(node->is_leaf() == false) {
          int parent_node_index = sorted_nodes.size() - 1;
          FillTreeNodes_(
            node->left(), parent_node_index, sorted_nodes,
            serialize_points_per_terminal_node_in, level + 1,
            add_serialize_points_per_terminal_node);
          FillTreeNodes_(
            node->right(), parent_node_index, sorted_nodes,
            serialize_points_per_terminal_node_in, level + 1,
            add_serialize_points_per_terminal_node);
        }
      }
    }

  public:

    /** @brief Returns the identifier information of the
     *         subtable. Currently (rank, begin, count) is the ID.
     */
    SubTableIDType subtable_id() const {
      return SubTableIDType(
               table_->rank(), start_node_->begin(), start_node_->count());
    }

    void set_start_node(TreeType *start_node_in) {
      start_node_ = start_node_in;
      serialize_points_per_terminal_node_.resize(0);
      serialize_points_per_terminal_node_.push_back(
        PointSerializeFlagType(start_node_in->begin(), start_node_in->count()));
    }

    bool serialize_new_from_old_mapping() const {
      return serialize_new_from_old_mapping_;
    }

    void set_cache_block_id(int cache_block_id_in) {
      cache_block_id_ = cache_block_id_in;
    }

    int cache_block_id() const {
      return cache_block_id_;
    }

    /** @brief Returns the list of terminal nodes for which the points
     *         underneath are available.
     */
    const std::vector <
    PointSerializeFlagType > &serialize_points_per_terminal_node() const {
      return serialize_points_per_terminal_node_;
    }

    /** @brief Manual destruction.
     */
    void Destruct() {
      if(is_alias_ == false && table_ != NULL) {
        if(core::table::global_m_file_) {
          core::table::global_m_file_->DestroyPtr(table_);
        }
        else {
          delete table_;
        }
      }
      is_alias_ = true;
      table_ = NULL;
    }

    /** @brief Returns whether the subtable is an alias of another
     *         subtable.
     */
    bool is_alias() const {
      return is_alias_;
    }

    void Alias(const SubTable<TableType> &subtable_in) {
      serialize_new_from_old_mapping_ =
        subtable_in.serialize_new_from_old_mapping();
      cache_block_id_ = subtable_in.cache_block_id();
      locked_mpi_rank_ = subtable_in.locked_mpi_rank();
      originating_rank_ = subtable_in.originating_rank();
      table_ = const_cast< SubTableType &>(subtable_in).table();
      start_node_ = const_cast< SubTableType &>(subtable_in).start_node();
      data_ = const_cast<SubTableType &>(subtable_in).data();
      weights_ = const_cast<SubTableType &>(subtable_in).weights();
      old_from_new_ = const_cast<SubTableType &>(subtable_in).old_from_new();
      new_from_old_ = const_cast<SubTableType &>(subtable_in).new_from_old();
      tree_ = const_cast<SubTableType &>(subtable_in).tree();
      is_alias_ = true;
      serialize_points_per_terminal_node_ =
        subtable_in.serialize_points_per_terminal_node();
    }

    /** @brief Steals the ownership of the incoming subtable.
     */
    void operator=(const SubTable<TableType> &subtable_in) {
      serialize_new_from_old_mapping_ =
        subtable_in.serialize_new_from_old_mapping();
      cache_block_id_ = subtable_in.cache_block_id();
      locked_mpi_rank_ = subtable_in.locked_mpi_rank();
      originating_rank_ = subtable_in.originating_rank();
      table_ = const_cast< SubTableType &>(subtable_in).table();
      start_node_ = const_cast< SubTableType &>(subtable_in).start_node();
      data_ = const_cast<SubTableType &>(subtable_in).data();
      weights_ = const_cast<SubTableType &>(subtable_in).weights();
      old_from_new_ = const_cast<SubTableType &>(subtable_in).old_from_new();
      new_from_old_ = const_cast<SubTableType &>(subtable_in).new_from_old();
      tree_ = const_cast<SubTableType &>(subtable_in).tree();
      is_alias_ = subtable_in.is_alias();
      const_cast<SubTableType &>(subtable_in).is_alias_ = true;
      serialize_points_per_terminal_node_ =
        subtable_in.serialize_points_per_terminal_node();
    }

    /** @brief Steals the ownership of the incoming subtable.
     */
    SubTable(const SubTable<TableType> &subtable_in) {
      this->operator=(subtable_in);
    }

    /** @brief Serialize the subtable.
     */
    template<class Archive>
    void save(Archive &ar, const unsigned int version) const {

      // Save the rank.
      int rank = table_->rank();
      ar & rank;

      // Save the tree.
      int num_nodes = 0;
      std::vector< std::pair<TreeType *, int> > tree_nodes;
      std::vector< PointSerializeFlagType >
      &serialize_points_per_terminal_node_alias =
        const_cast< std::vector<PointSerializeFlagType> & >(
          serialize_points_per_terminal_node_);
      FillTreeNodes_(
        start_node_, -1, tree_nodes,
        &serialize_points_per_terminal_node_alias, 0, is_alias_);
      num_nodes = tree_nodes.size();
      ar & num_nodes;
      for(unsigned int i = 0; i < tree_nodes.size(); i++) {
        ar & (*(tree_nodes[i].first));
        ar & tree_nodes[i].second;
      }

      // Save the node ids for which there are points available
      // underneath.
      int serialize_points_per_terminal_node_size =
        static_cast<int>(serialize_points_per_terminal_node_.size());
      ar & serialize_points_per_terminal_node_size;
      for(unsigned int i = 0;
          i < serialize_points_per_terminal_node_.size(); i++) {
        ar & serialize_points_per_terminal_node_[i];
      }

      // Save the matrix and the mappings if requested.
      {
        core::table::SubDenseMatrix<SubTableType> sub_data;
        core::table::SubDenseMatrix<SubTableType> sub_weights;

        // If the subtable is an alias, specify which subset to save.
        if(is_alias_) {
          sub_data.Init(data_, serialize_points_per_terminal_node_);
          sub_weights.Init(weights_, serialize_points_per_terminal_node_);
        }

        // Otherwise, we save the entire thing.
        else {
          sub_data.Init(data_);
          sub_weights.Init(weights_);
        }
        ar & sub_data;
        ar & sub_weights;

        // Direct mapping saving.
        core::table::IndexUtil<OldFromNewIndexType>::Serialize(
          ar, old_from_new_->get(),
          serialize_points_per_terminal_node_, is_alias_, false);

        // Save whether the new from old mapping is going to be
        // serialized or not.
        ar & serialize_new_from_old_mapping_;
        if(serialize_new_from_old_mapping_) {
          core::table::IndexUtil<int>::Serialize(
            ar, new_from_old_->get(),
            serialize_points_per_terminal_node_, is_alias_, false);
        }
      }
    }

    /** @brief Unserialize the subtable.
     */
    template<class Archive>
    void load(Archive &ar, const unsigned int version) {

      // Set the rank.
      int rank_in;
      ar & rank_in;
      table_->set_rank(rank_in);

      // Load up the max number of loads to receive.
      int num_nodes;
      ar & num_nodes;
      std::vector< std::pair<TreeType *, int> > tree_nodes(num_nodes);
      for(int i = 0; i < num_nodes; i++) {
        tree_nodes[i].first =
          (core::table::global_m_file_) ?
          core::table::global_m_file_->Construct<TreeType>() : new TreeType();
        ar & (*(tree_nodes[i].first));
        ar & tree_nodes[i].second;
      }

      // Do the pointer corrections, and have the tree point to the
      // 0-th element.
      for(unsigned int i = 1; i < tree_nodes.size(); i++) {
        int parent_node_index = tree_nodes[i].second;
        if(tree_nodes[parent_node_index].first->begin() ==
            tree_nodes[i].first->begin()) {
          tree_nodes[parent_node_index].first->set_left_child(
            (*data_), tree_nodes[i].first);
        }
        else {
          tree_nodes[parent_node_index].first->set_right_child(
            (*data_), tree_nodes[i].first);
        }
      }
      (*tree_) = tree_nodes[0].first;
      start_node_ = tree_nodes[0].first;

      // Load the node ids for which there are points underneath.
      table_->set_entire_points_available(false);
      int serialize_points_per_terminal_node_size;
      ar & serialize_points_per_terminal_node_size;
      serialize_points_per_terminal_node_.resize(
        serialize_points_per_terminal_node_size);
      for(int i = 0; i < serialize_points_per_terminal_node_size; i++) {
        ar & serialize_points_per_terminal_node_[i];

        // Add the list of points that are serialized to the table so
        // that the iterators work properly.
        table_->add_begin_count_pairs(
          serialize_points_per_terminal_node_[i].begin(),
          serialize_points_per_terminal_node_[i].count());
      }

      // Load the data and the mappings if available.
      {
        core::table::SubDenseMatrix<SubTableType> sub_data;
        sub_data.Init(data_, serialize_points_per_terminal_node_);
        ar & sub_data;
        core::table::SubDenseMatrix<SubTableType> sub_weights;
        sub_weights.Init(weights_, serialize_points_per_terminal_node_);
        ar & sub_weights;
        if(table_->mappings_are_aliased() == false) {
          (*old_from_new_) =
            (core::table::global_m_file_) ?
            core::table::global_m_file_->ConstructArray <
            OldFromNewIndexType > (data_->n_cols()) :
            new OldFromNewIndexType[ data_->n_cols()];
          (*new_from_old_) =
            (core::table::global_m_file_) ?
            core::table::global_m_file_->ConstructArray <
            int > (data_->n_cols()) : new int[ data_->n_cols()] ;
        }

        // Always serialize onto a consecutive block of memory to save
        // space.
        core::table::IndexUtil<OldFromNewIndexType>::Serialize(
          ar, old_from_new_->get(),
          serialize_points_per_terminal_node_, true, true);

        // Find out whether the new from old mapping was serialized or
        // not, and load accordingly.
        ar & serialize_new_from_old_mapping_;
        if(serialize_new_from_old_mapping_) {
          core::table::IndexUtil<int>::Serialize(
            ar, new_from_old_->get(),
            serialize_points_per_terminal_node_, true, true);
        }
      }
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()

    void set_originating_rank(int originating_rank_in) {
      originating_rank_ = originating_rank_in;
    }

    /** @brief The default constructor.
     */
    SubTable() {
      serialize_new_from_old_mapping_ = true;
      cache_block_id_ = 0;
      locked_mpi_rank_ = -1;
      originating_rank_ = -1;
      table_ = NULL;
      start_node_ = NULL;
      data_ = NULL;
      weights_ = NULL;
      old_from_new_ = NULL;
      new_from_old_ = NULL;
      tree_ = NULL;
      is_alias_ = true;
    }

    /** @brief The destructor.
     */
    ~SubTable() {
      if(is_alias_ == false && table_ != NULL) {
        if(core::table::global_m_file_) {
          core::table::global_m_file_->DestroyPtr(table_);
        }
        else {
          delete table_;
        }
      }
    }

    int locked_mpi_rank() const {
      return locked_mpi_rank_;
    }

    bool is_locked() const {
      return locked_mpi_rank_ >= 0;
    }

    void Unlock() {
      locked_mpi_rank_ = -1;
    }

    void Lock(int mpi_rank_in) {
      locked_mpi_rank_ = mpi_rank_in;
    }

    /** @brief Returns the underlying table object.
     */
    TableType *table() const {
      return table_;
    }

    /** @brief Returns the starting node to be serialized.
     */
    TreeType *start_node() const {
      return start_node_;
    }

    /** @brief Returns the underlying weights.
     */
    core::table::DenseMatrix *weights() const {
      return weights_;
    }

    /** @brief Returns the underlying multi-dimensional data.
     */
    core::table::DenseMatrix *data() const {
      return data_;
    }

    int originating_rank() const {
      return originating_rank_;
    }

    /** @brief Returns the old_from_new mapping.
     */
    boost::interprocess::offset_ptr <
    OldFromNewIndexType > *old_from_new() const {
      return old_from_new_;
    }

    /** @brief Returns the new_from_old mapping.
     */
    boost::interprocess::offset_ptr<int> *new_from_old() const {
      return new_from_old_;
    }

    /** @brief Returns the tree owned by the subtable.
     */
    boost::interprocess::offset_ptr<TreeType> *tree() const {
      return tree_;
    }

    bool has_same_subtable_id(const std::pair<int, int> &sub_table_id) const {
      return start_node_->begin() == sub_table_id.first &&
             start_node_->count() == sub_table_id.second;
    }

    /** @brief Initializes a subtable before loading.
     */
    void Init(
      int cache_block_id_in,
      bool serialize_new_from_old_mapping_in) {

      // Set the cache block ID.
      cache_block_id_ = cache_block_id_in;

      // Allocate the table.
      table_ = (core::table::global_m_file_) ?
               core::table::global_m_file_->Construct<TableType>() :
               new TableType();

      // Finalize the intialization.
      this->Init(
        table_, (TreeType *) NULL, serialize_new_from_old_mapping_in);

      // Since table_ pointer is explicitly allocated, is_alias_ flag
      // is turned to false. It is important that it is here to
      // overwrite is_alias_ flag after ALL initializations are done.
      is_alias_ = false;
    }

    /** @brief Initializes a subtable from a pre-existing table before
     *         serializing a subset of it.
     */
    void Init(
      TableType *table_in, TreeType *start_node_in,
      bool serialize_new_from_old_mapping_in) {
      serialize_new_from_old_mapping_ = serialize_new_from_old_mapping_in;
      table_ = table_in;
      is_alias_ = true;
      originating_rank_ = table_->rank();
      start_node_ = start_node_in;
      data_ = &(table_in->data());
      weights_ = &(table_in->weights());
      old_from_new_ = table_in->old_from_new_offset_ptr();
      new_from_old_ = table_in->new_from_old_offset_ptr();
      tree_ = table_in->get_tree_offset_ptr();
    }
};
}
}

#endif
