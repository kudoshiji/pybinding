#pragma once
#include "numeric/dense.hpp"
#include <vector>
#include <string>
#include <unordered_map>

namespace cpb {

// TODO: replace with proper string_view
using string_view = std::string const&;

/// Sublattice and hopping ID data types
using sub_id = std::int8_t;
using hop_id = std::int8_t;

/**
 Helper class for passing sublattice information to modifier functions
 */
struct SubIdRef {
    ArrayX<sub_id> const& ids;
    std::unordered_map<std::string, sub_id> name_map;
};

/**
 Helper class for passing hopping information to modifier functions
 */
struct HopIdRef {
    ArrayX<hop_id> const& ids;
    std::unordered_map<std::string, hop_id> name_map;
};

class OptimizedUnitCell;

/**
 Crystal lattice specification
 */
class Lattice {
public:
    struct Sublattice {
        Cartesian position; ///< relative to lattice origin
        MatrixXcd energy; ///< onsite energy term
        sub_id unique_id; ///< different for each entry
        sub_id alias_id; ///< may be shared by multiple entries, e.g. for creating supercells

        /// Return the onsite energy vector (matrix diagonal) in the given scalar type
        template<class scalar_t>
        VectorX<scalar_t> energy_vector_as() const;
    };

    struct HoppingTerm {
        Index3D relative_index; ///< relative index between two unit cells - may be (0, 0, 0)
        sub_id from; ///< source sublattice unique ID
        sub_id to; ///< destination sublattice unique ID

        friend bool operator==(HoppingTerm const& a, HoppingTerm const& b) {
            auto const left = std::tie(a.relative_index, a.from, a.to);
            auto const right = std::tie(b.relative_index, b.from, b.to);
            auto const right_conjugate = std::tie(-b.relative_index, b.to, b.from);
            return left == right || left == right_conjugate;
        }

        friend bool operator!=(HoppingTerm const& a, HoppingTerm const& b) { return !(a == b); }
    };

    struct HoppingFamily {
        MatrixXcd energy; ///< base hopping energy which is shared by all terms in this family
        hop_id family_id; ///< different for each family
        std::vector<HoppingTerm> terms; ///< site connections

        /// Return the hopping energy matrix in the given scalar type
        template<class scalar_t>
        MatrixX<scalar_t> energy_matrix_as() const;
    };

    using Vectors = std::vector<Cartesian>;
    using Sublattices = std::unordered_map<std::string, Sublattice>;
    using Hoppings = std::unordered_map<std::string, HoppingFamily>;
    using NameMap = std::unordered_map<std::string, std::int8_t>;

public:
    Lattice(Cartesian a1, Cartesian a2 = {0, 0, 0}, Cartesian a3 = {0, 0, 0});
    Lattice(Vectors v, Sublattices s, Hoppings h)
        : vectors(std::move(v)), sublattices(std::move(s)), hoppings(std::move(h)) {}

    /// Create a new sublattice
    void add_sublattice(string_view name, Cartesian position, double onsite_energy = .0);
    void add_sublattice(string_view name, Cartesian position, VectorXd const& onsite_energy);
    void add_sublattice(string_view name, Cartesian position, MatrixXcd const& onsite_energy);

    /// Create a sublattice which an alias for an existing lattice at a different position
    void add_alias(string_view alias_name, string_view original_name, Cartesian position);

    /// Associate a name with a hopping energy, but don't connect any sites
    void register_hopping_energy(string_view name, std::complex<double> energy);
    void register_hopping_energy(string_view name, MatrixXcd const& energy);

    /// Connect sites with an already registered hopping name/energy
    void add_hopping(Index3D relative_index, string_view from_sub, string_view to_sub,
                     string_view hopping_family_name);

    /// Connect sites with an anonymous hopping energy
    void add_hopping(Index3D relative_index, string_view from_sub, string_view to_sub,
                     std::complex<double> energy);
    void add_hopping(Index3D relative_index, string_view from_sub, string_view to_sub,
                     MatrixXcd const& energy);

public: // getters and setters
    /// The primitive vectors that define the lattice
    Vectors const& get_vectors() const { return vectors; }
    /// All the sites that belong to the primitive cell
    Sublattices const& get_sublattices() const { return sublattices; }
    /// Connections inside the unit cell or to a neighboring cell
    Hoppings const& get_hoppings() const { return hoppings; }

    /// The lattice offset value is added to the positions of all the sublattices
    Cartesian get_offset() const { return offset; }
    /// Set the lattice offset -- it must be within half the length of a primitive vector
    void set_offset(Cartesian position);
    /// Return a copy of this lattice with a different offset
    Lattice with_offset(Cartesian position) const;

    /// Any site which has less neighbors than this minimum will be considered as "dangling"
    int get_min_neighbors() const { return min_neighbors; }
    /// Set the minimum number of neighbors for any site
    void set_min_neighbors(int n) { min_neighbors = n; }
    /// Return a copy of this lattice with a different minimum neighbor count
    Lattice with_min_neighbors(int number) const;

public: // properties
    /// The dimensionality of the lattice
    int ndim() const { return static_cast<int>(vectors.size()); }
    /// Number of sublattices
    int nsub() const { return static_cast<int>(sublattices.size()); }
    /// Number of hopping families
    int nhop() const { return static_cast<int>(hoppings.size()); }

    /// Get a single vector -- Expects: index < lattice.ndim()
    Cartesian vector(size_t index) const { return vectors[index]; }

    /// Access sublattice information by name or ID
    Sublattice const& sublattice(string_view name) const;
    Sublattice const& sublattice(sub_id id) const;
    Sublattice const& operator[](string_view name) const { return sublattice(name); }
    Sublattice const& operator[](sub_id id) const { return sublattice(id); }
    /// Assess hopping family information by name or ID
    HoppingFamily const& hopping_family(string_view name) const;
    HoppingFamily const& hopping_family(hop_id id) const;
    HoppingFamily const& operator()(string_view name) const { return hopping_family(name); }
    HoppingFamily const& operator()(hop_id id) const { return hopping_family(id); }

    /// Get the maximum possible number of hoppings from any site of this lattice
    int max_hoppings() const;
    /// Does at least one sublattice have non-zero onsite energy?
    bool has_onsite_energy() const;
    /// Does at least one sublattice have multiple orbitals?
    bool has_multiple_orbitals() const;
    /// Is at least one hopping complex?
    bool has_complex_hoppings() const;

public: // optimized access
    /// Return the optimized version of the lattice structure -- see `OptimizedLatticeStructure`
    OptimizedUnitCell optimized_unit_cell() const;

    /// Mapping from friendly sublattice names to their unique IDs
    NameMap sub_name_map() const;
    /// Mapping from friendly hopping names to their unique IDs
    NameMap hop_name_map() const;

public: // utilities
    /// Calculate the spatial position of a unit cell or a sublattice site if specified
    Cartesian calc_position(Index3D index, string_view sublattice_name = "") const;

    /**
     Translate Cartesian `position` into lattice vector coordinates

     Return vector {n1, n2, n3} which satisfies `position = n1*a1 + n2*a2 + n3*a3`,
     where a1, a2 and a3 are lattice vectors. Note that the return vector is `float`.
     */
    Vector3f translate_coordinates(Cartesian position) const;

private:
    sub_id register_sublattice(string_view name);

private:
    Vectors vectors; ///< primitive vectors that define the lattice
    Sublattices sublattices; ///< i.e the sites that belong to the primitive cell
    Hoppings hoppings; ///< connections inside the unit cell or to a neighboring cell
    Cartesian offset = {0, 0, 0}; ///< global offset: sublattices are defined relative to this
    int min_neighbors = 1; ///< minimum number of neighbours required at each lattice site
};

/**
 Unit cell data for a `Lattice` (all sites and hopping connections)

 The data layout is optimized for traversal over sites. In addition, the sites
 are sorted by the size of the onsite energy matrix and the alias ID.
 */
class OptimizedUnitCell {
public:
    /// Similar to Lattice::HoppingTerm but `from_sublattice` is implicit
    struct Hopping {
        Index3D relative_index;
        storage_idx_t to_sublattice; ///< destination sublattice index in `sites` (not `unique_id`)
        hop_id family_id;
        bool is_conjugate; ///< true if this is an automatically added complex conjugate term
    };

    /// Similar to Lattice::Sublattice but each site lists hopping from itself
    struct Site {
        Cartesian position;
        storage_idx_t norb; ///< number of orbitals on this site
        sub_id unique_id;
        sub_id alias_id;
        std::vector<Hopping> hoppings;
    };

    using Sites = std::vector<Site>;

public:
    explicit OptimizedUnitCell(Lattice const& lattice);

    Site const& operator[](size_t n) const { return sites[n]; }
    Sites::const_iterator begin() const { return sites.begin(); }
    Sites::const_iterator end() const { return sites.end(); }

private:
    Sites sites;
};

template<class scalar_t>
VectorX<scalar_t> Lattice::Sublattice::energy_vector_as() const {
    return energy.diagonal().cast<scalar_t>();
}

template<>
inline VectorX<double> Lattice::Sublattice::energy_vector_as<double>() const {
    return energy.diagonal().real();
}

template<>
inline VectorX<float> Lattice::Sublattice::energy_vector_as<float>() const {
    return energy.diagonal().real().cast<float>();
}

template<class scalar_t>
MatrixX<scalar_t> Lattice::HoppingFamily::energy_matrix_as() const {
    return energy.cast<scalar_t>();
}

template<>
inline MatrixX<double> Lattice::HoppingFamily::energy_matrix_as<double>() const {
    return energy.real();
}

template<>
inline MatrixX<float> Lattice::HoppingFamily::energy_matrix_as<float>() const {
    return energy.real().cast<float>();
}

} // end namespace cpb