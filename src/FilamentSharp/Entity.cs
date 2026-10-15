namespace FilamentSharp;

/// <summary>
/// An entity: a lightweight 32-bit identifier that can carry Filament
/// components (camera, transform, renderable, light). Entities are created
/// from the global <see cref="EntityManager"/> and are not owned by any
/// engine.
/// </summary>
/// <remarks>
/// The zero entity (<see cref="Entity.Null"/>) carries no components.
/// Destroying an entity removes all its components; destroy it only after
/// removing it from every scene (or let scene teardown handle it).
/// </remarks>
public readonly struct Entity : IEquatable<Entity>
{
    /// <summary>The null entity.</summary>
    public static readonly Entity Null;

    internal Entity(uint id)
    {
        Id = id;
    }

    /// <summary>The raw entity id.</summary>
    public uint Id { get; }

    /// <summary>Gets a value indicating whether this is a non-null entity.</summary>
    public bool IsValid => Id != 0;

    /// <inheritdoc />
    public bool Equals(Entity other) => Id == other.Id;

    /// <inheritdoc />
    public override bool Equals(object? obj) => obj is Entity other && Equals(other);

    /// <inheritdoc />
    public override int GetHashCode() => Id.GetHashCode();

    /// <inheritdoc />
    public override string ToString() => $"Entity({Id})";

    /// <summary>Determines whether two entities have the same id.</summary>
    /// <param name="left">Left operand.</param>
    /// <param name="right">Right operand.</param>
    /// <returns><c>true</c> when the ids are equal.</returns>
    public static bool operator ==(Entity left, Entity right) => left.Equals(right);

    /// <summary>Determines whether two entities have different ids.</summary>
    /// <param name="left">Left operand.</param>
    /// <param name="right">Right operand.</param>
    /// <returns><c>true</c> when the ids differ.</returns>
    public static bool operator !=(Entity left, Entity right) => !left.Equals(right);
}

/// <summary>
/// The global Filament entity manager. Entities are process-wide; they are
/// not tied to a specific engine.
/// </summary>
public static class EntityManager
{
    /// <summary>Creates a new, component-less entity.</summary>
    /// <returns>A valid, non-null entity.</returns>
    public static Entity Create()
    {
        return new Entity(Native.EntityCreate());
    }

    /// <summary>
    /// Destroys an entity and removes every Filament component from it.
    /// Destroying <see cref="Entity.Null"/> is a no-op. Destroying an entity
    /// twice is safe.
    /// </summary>
    public static void Destroy(Entity entity)
    {
        Native.EntityDestroy(entity.Id);
    }
}
