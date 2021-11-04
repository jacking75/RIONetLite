using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace csharp_test_client
{
    class PacketDef
    {
        public const Int16 PACKET_HEADER_SIZE = 5;
        public const int MAX_USER_ID_BYTE_LENGTH = 20;
        public const int MAX_USER_PW_BYTE_LENGTH = 20;
    }

    public enum PACKET_ID : ushort
    {
        PACKET_ID_ECHO = 101,

        // Ping(Heart-beat)
        PACKET_ID_PING_REQ = 106,
        PACKET_ID_PING_RES = 107,

        PACKET_ID_ERROR_NTF = 110,


        // 로그인
        PACKET_ID_LOGIN_REQ = 201,
        PACKET_ID_LOGIN_RES = 202,
                
        PACKET_ID_ROOM_ENTER_REQ = 206,
        PACKET_ID_ROOM_ENTER_RES = 207,
        PACKET_ID_ROOM_USER_LIST_NTF = 208,
        PACKET_ID_ROOM_NEW_USER_NTF = 209,

         PACKET_ID_ROOM_LEAVE_REQ = 216,
         PACKET_ID_ROOM_LEAVE_RES = 217,
         PACKET_ID_ROOM_LEAVE_USER_NTF = 218,

         PACKET_ID_ROOM_CHAT_REQ = 221,
         PACKET_ID_ROOM_CHAT_RES = 222,
         PACKET_ID_ROOM_CHAT_NOTIFY = 223,
    }


    public enum ERROR_CODE : Int16
    {
        ERROR_NONE = 0,



        ERROR_CODE_USER_MGR_INVALID_USER_UNIQUEID = 112,

        ERROR_CODE_PUBLIC_CHANNEL_IN_USER = 114,

        ERROR_CODE_PUBLIC_CHANNEL_INVALIDE_NUMBER = 115,
    }
}
