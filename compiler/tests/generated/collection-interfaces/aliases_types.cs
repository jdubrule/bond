

// suppress "Missing XML comment for publicly visible type or member"
#pragma warning disable 1591


#region ReSharper warnings
// ReSharper disable PartialTypeWithSinglePart
// ReSharper disable RedundantNameQualifier
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace
// ReSharper disable UnusedParameter.Local
// ReSharper disable RedundantUsingDirective
#endregion

namespace tests
{
    using System.Collections.Generic;

    [global::Bond.Schema]
    [System.CodeDom.Compiler.GeneratedCode("gbc", "0.7.0.0")]
    public partial class Foo<T>
    {
        [global::Bond.Id(0), global::Bond.Type(typeof(List<List<global::Bond.Tag.classT>>))]
        public IList<IList<T>> aa { get; set; }

        public Foo()
            : this("tests.Foo", "Foo")
        {}

        protected Foo(string fullName, string name)
        {
            aa = new List<IList<T>>();
        }
    }
} // tests
