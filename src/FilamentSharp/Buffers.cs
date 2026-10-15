using System.Runtime.InteropServices;

namespace FilamentSharp;

// The enums below intentionally mirror the native Filament enums
// (VertexAttribute, backend::ElementType) member-for-member: interop fidelity
// is more valuable here than the generic naming rules CA1711/CA1720, which
// are suppressed for these types with that justification.
#pragma warning disable CA1711 // enum name mirrors filament::VertexAttribute
#pragma warning disable CA1720 // member names mirror backend::ElementType

/// <summary>Vertex attribute identifiers (values mirror filament's VertexAttribute).</summary>
public enum VertexAttribute
{
    /// <summary>XYZ position (typically <see cref="ElementType.Float3"/>).</summary>
    Position = 0,

    /// <summary>Tangent space encoded as a quaternion (float4).</summary>
    Tangents = 1,

    /// <summary>Vertex color (float4).</summary>
    Color = 2,

    /// <summary>First texture coordinate set (typically float2).</summary>
    Uv0 = 3,

    /// <summary>Second texture coordinate set (typically float2).</summary>
    Uv1 = 4,

    /// <summary>Bone indices (uvec4).</summary>
    BoneIndices = 5,

    /// <summary>Bone weights (normalized float4).</summary>
    BoneWeights = 6,

    /// <summary>Custom attribute slot 0.</summary>
    Custom0 = 8,

    /// <summary>Custom attribute slot 1.</summary>
    Custom1 = 9,

    /// <summary>Custom attribute slot 2.</summary>
    Custom2 = 10,

    /// <summary>Custom attribute slot 3.</summary>
    Custom3 = 11,

    /// <summary>Custom attribute slot 4.</summary>
    Custom4 = 12,

    /// <summary>Custom attribute slot 5.</summary>
    Custom5 = 13,

    /// <summary>Custom attribute slot 6.</summary>
    Custom6 = 14,

    /// <summary>Custom attribute slot 7.</summary>
    Custom7 = 15,
}

/// <summary>
/// Vertex attribute element types (values mirror filament's
/// <c>backend::ElementType</c>).
/// </summary>
public enum ElementType
{
    /// <summary>One signed byte.</summary>
    Byte = 0,
    /// <summary>Two signed bytes.</summary>
    Byte2 = 1,
    /// <summary>Three signed bytes.</summary>
    Byte3 = 2,
    /// <summary>Four signed bytes.</summary>
    Byte4 = 3,
    /// <summary>One unsigned byte.</summary>
    UByte = 4,
    /// <summary>Two unsigned bytes.</summary>
    UByte2 = 5,
    /// <summary>Three unsigned bytes.</summary>
    UByte3 = 6,
    /// <summary>Four unsigned bytes.</summary>
    UByte4 = 7,
    /// <summary>One signed short.</summary>
    Short = 8,
    /// <summary>Two signed shorts.</summary>
    Short2 = 9,
    /// <summary>Three signed shorts.</summary>
    Short3 = 10,
    /// <summary>Four signed shorts.</summary>
    Short4 = 11,
    /// <summary>One unsigned short.</summary>
    UShort = 12,
    /// <summary>Two unsigned shorts.</summary>
    UShort2 = 13,
    /// <summary>Three unsigned shorts.</summary>
    UShort3 = 14,
    /// <summary>Four unsigned shorts.</summary>
    UShort4 = 15,
    /// <summary>One 32-bit signed integer.</summary>
    Int = 16,
    /// <summary>One 32-bit unsigned integer.</summary>
    UInt = 17,
    /// <summary>One 32-bit float.</summary>
    Float = 18,
    /// <summary>Two 32-bit floats.</summary>
    Float2 = 19,
    /// <summary>Three 32-bit floats.</summary>
    Float3 = 20,
    /// <summary>Four 32-bit floats.</summary>
    Float4 = 21,
    /// <summary>One half-precision float.</summary>
    Half = 22,
    /// <summary>Two half-precision floats.</summary>
    Half2 = 23,
    /// <summary>Three half-precision floats.</summary>
    Half3 = 24,
    /// <summary>Four half-precision floats.</summary>
    Half4 = 25,
}

/// <summary>
/// Declares one vertex attribute for
/// <see cref="Engine.CreateVertexBuffer(uint, uint, ReadOnlySpan{VertexAttributeDescription})"/>.
/// The layout mirrors the native <c>fs_vertex_attribute_desc</c> (20 bytes;
/// verified by compile-time checks in the native tests).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct VertexAttributeDescription
{
    /// <summary>The attribute this declaration describes.</summary>
    public VertexAttribute Attribute;

    /// <summary>The element type of the attribute data.</summary>
    public ElementType ElementType;

    /// <summary>The index of the data buffer the attribute reads from.</summary>
    public uint BufferIndex;

    /// <summary>Byte offset of the attribute within its buffer.</summary>
    public uint ByteOffset;

    /// <summary>Byte stride between consecutive vertices (0 = tightly packed).</summary>
    public uint ByteStride;

    /// <summary>Creates an attribute declaration.</summary>
    /// <param name="attribute">The vertex attribute.</param>
    /// <param name="elementType">The element type.</param>
    /// <param name="bufferIndex">The source buffer index.</param>
    /// <param name="byteOffset">Byte offset inside the buffer.</param>
    /// <param name="byteStride">Byte stride (0 = tightly packed).</param>
    public VertexAttributeDescription(VertexAttribute attribute, ElementType elementType,
        uint bufferIndex = 0, uint byteOffset = 0, uint byteStride = 0)
    {
        Attribute = attribute;
        ElementType = elementType;
        BufferIndex = bufferIndex;
        ByteOffset = byteOffset;
        ByteStride = byteStride;
    }
}

/// <summary>Index buffer element size.</summary>
public enum IndexType
{
    /// <summary>16-bit indices.</summary>
    UnsignedShort = 0,

    /// <summary>32-bit indices.</summary>
    UnsignedInt = 1,
}

/// <summary>
/// A vertex buffer: typed, interleaved-or-planar vertex data uploaded to the
/// GPU. Attributes are declared at creation and data is uploaded per buffer
/// index with <see cref="SetBufferAt(uint, ReadOnlySpan{byte})"/>.
/// </summary>
public sealed class VertexBuffer : FilamentObject
{
    internal VertexBuffer(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>
    /// Uploads the data for one buffer index. The native side copies the
    /// data; the span is not retained after the call.
    /// </summary>
    /// <param name="bufferIndex">Which of the declared buffers to upload.</param>
    /// <param name="data">The raw bytes.</param>
    public void SetBufferAt(uint bufferIndex, ReadOnlySpan<byte> data)
    {
        ThrowIfDisposed();
        if (data.IsEmpty)
        {
            throw new ArgumentException("Buffer data must not be empty.", nameof(data));
        }
        Native.VertexBufferSetBuffer(Engine.DangerousGetHandle(), RawHandle,
            bufferIndex, data, (nuint)data.Length).Check(nameof(VertexBuffer.SetBufferAt));
    }
}

/// <summary>An index buffer referencing vertices of a <see cref="VertexBuffer"/>.</summary>
public sealed class IndexBuffer : FilamentObject
{
    internal IndexBuffer(Engine engine, ChildSafeHandle handle)
        : base(engine, handle)
    {
    }

    /// <summary>
    /// Uploads the index data. The native side copies the data; the span is
    /// not retained after the call.
    /// </summary>
    /// <param name="data">Raw bytes: <c>index_count * element_size</c>.</param>
    public void SetBuffer(ReadOnlySpan<byte> data)
    {
        ThrowIfDisposed();
        if (data.IsEmpty)
        {
            throw new ArgumentException("Buffer data must not be empty.", nameof(data));
        }
        Native.IndexBufferSetBuffer(Engine.DangerousGetHandle(), RawHandle,
            data, (nuint)data.Length).Check(nameof(IndexBuffer.SetBuffer));
    }
}

#pragma warning restore CA1720
#pragma warning restore CA1711
