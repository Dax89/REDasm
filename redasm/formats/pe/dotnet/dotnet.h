#ifndef PEDOTNET_H
#define PEDOTNET_H

#include "dotnet_header.h"
#include "dotnet_tables.h"

namespace REDasm {

class PeDotNet
{
    private:
        typedef std::function<void(u32**, const CorTables&, CorTable&)> TableCallback;
        typedef std::unordered_map<u32, TableCallback> TableDispatcher;

    private:
        PeDotNet();

    public:
        static std::string getVersion(ImageCor20MetaData *cormetadata);
        static u16 getNumberOfStreams(ImageCor20MetaData* cormetadata);
        static ImageStreamHeader* getStream(ImageCor20MetaData* cormetadata, const std::string &id);
        static bool getTables(ImageCor20TablesHeader* cortablesheader, CorTables& tables);

    private:
        static u32 getSizeOfHeap(ImageCor20TablesHeader *cortablesheader, u32 bitno);
        static u32 getValueIdx(u32** data, u32 offsize);
        static u32 getTableIdx(u32** data, const CorTables& tables, u32 table);
        static u32 getStringIdx(u32** data, const CorTables& tables);
        static u32 getGuidIdx(u32** data, const CorTables& tables);
        static u32 getBlobIdx(u32** data, const CorTables& tables);
        static void getModule(u32** data, const CorTables& tables, CorTable& table);
        static void getTypeRef(u32** data, const CorTables& tables, CorTable& table);
        static void getTypeDef(u32** data, const CorTables& tables, CorTable& table);
        static void getFieldDef(u32** data, const CorTables& tables, CorTable& table);
        static void getMethodDef(u32** data, const CorTables& tables, CorTable& table);
        static void getParamDef(u32** data, const CorTables& tables, CorTable& table);
        static void getInterfaceImpl(u32** data, const CorTables& tables, CorTable& table);
        static void getMemberRef(u32** data, const CorTables& tables, CorTable& table);
        static void getConstant(u32** data, const CorTables& tables, CorTable& table);
        static void getCustomAttribute(u32** data, const CorTables& tables, CorTable& table);
        static void getFieldMarshal(u32** data, const CorTables& tables, CorTable& table);
        static void getDeclSecurity(u32** data, const CorTables& tables, CorTable& table);
        static void getClassLayout(u32** data, const CorTables& tables, CorTable& table);
        static void getFieldLayout(u32** data, const CorTables& tables, CorTable& table);
        static void getStandaloneSig(u32** data, const CorTables& tables, CorTable& table);
        static void getEventMap(u32** data, const CorTables& tables, CorTable& table);
        static void getEvent(u32** data, const CorTables& tables, CorTable& table);
        static void getPropertyMap(u32** data, const CorTables& tables, CorTable& table);
        static void getProperty(u32** data, const CorTables& tables, CorTable& table);
        static void getMethodSemantics(u32** data, const CorTables& tables, CorTable& table);
        static void getMethodImpl(u32** data, const CorTables& tables, CorTable& table);
        static void getModuleRef(u32** data, const CorTables& tables, CorTable& table);
        static void getTypeSpec(u32** data, const CorTables& tables, CorTable& table);
        static void getImplMap(u32** data, const CorTables& tables, CorTable& table);
        static void getFieldRVA(u32** data, const CorTables& tables, CorTable& table);
        static void getAssembly(u32** data, const CorTables& tables, CorTable& table);
        static void getAssemblyProcessor(u32** data, const CorTables& tables, CorTable& table);
        static void getAssemblyOS(u32** data, const CorTables& tables, CorTable& table);
        static void getAssemblyRef(u32** data, const CorTables& tables, CorTable& table);
        static void getAssemblyRefProcessor(u32** data, const CorTables& tables, CorTable& table);
        static void getAssemblyRefOS(u32** data, const CorTables& tables, CorTable& table);
        static void getFile(u32** data, const CorTables& tables, CorTable& table);
        static void getExportedType(u32** data, const CorTables& tables, CorTable& table);
        static void getManifestResource(u32** data, const CorTables& tables, CorTable& table);
        static void getNestedClass(u32** data, const CorTables& tables, CorTable& table);
        static void getGenericParam(u32** data, const CorTables& tables, CorTable& table);
        static void getGenericParamConstraint(u32** data, const CorTables& tables, CorTable& table);
        static void initTables();

    private:
        static void getTaggedField(u32** data, u32& value, u8& tag, u8 tagbits, const CorTables& tables, const std::list<u32>& tablerefs);
        static u32 maxRows( const CorTables& tables, const std::list<u32>& tablerefs);

    private:
        static std::list<u32> _tables;
        static TableDispatcher _dispatcher;
};

} // namespace REDasm

#endif // PEDOTNET_H
