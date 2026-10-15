using System.Runtime.InteropServices;

namespace FilamentSharp;

/// <summary>
/// Exception thrown when a native FilamentSharp operation fails. The message
/// carries the error text retrieved from the native bridge's thread-local
/// error state.
/// </summary>
public sealed class FilamentSharpException : Exception
{
    /// <summary>Creates an exception with a native error message.</summary>
    /// <param name="message">The combined operation/error description.</param>
    public FilamentSharpException(string message)
        : base(message)
    {
    }
}
