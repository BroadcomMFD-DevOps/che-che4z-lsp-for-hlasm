/*
 * Copyright (c) 2021 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "diagnostic.h"
#include "diagnostic_consumer.h"
#include "document.h"
#include "lexing/logical_line.h"
#include "preprocessor_options.h"
#include "preprocessor_utils.h"
#include "processing/preprocessor.h"
#include "protocol.h"
#include "range.h"
#include "semantics/source_info_processor.h"
#include "semantics/statement.h"
#include "utils/concat.h"
#include "utils/string_operations.h"
#include "utils/unicode_text.h"
#include "workspaces/parse_lib_provider.h"

namespace hlasm_plugin::parser_library::processing {

namespace {

using utils::concat;

const std::unordered_map<std::string_view, int> DFHRESP_operands = {
    { "NORMAL", 0 },
    { "ERROR", 1 },
    { "RDATT", 2 },
    { "WRBRK", 3 },
    { "EOF", 4 },
    { "EODS", 5 },
    { "EOC", 6 },
    { "INBFMH", 7 },
    { "ENDINPT", 8 },
    { "NONVAL", 9 },
    { "NOSTART", 10 },
    { "TERMIDERR", 11 },
    { "DSIDERR", 12 },
    { "FILENOTFOUND", 12 },
    { "NOTFND", 13 },
    { "DUPREC", 14 },
    { "DUPKEY", 15 },
    { "INVREQ", 16 },
    { "IOERR", 17 },
    { "NOSPACE", 18 },
    { "NOTOPEN", 19 },
    { "ENDFILE", 20 },
    { "ILLOGIC", 21 },
    { "LENGERR", 22 },
    { "QZERO", 23 },
    { "SIGNAL", 24 },
    { "QBUSY", 25 },
    { "ITEMERR", 26 },
    { "PGMIDERR", 27 },
    { "TRANSIDERR", 28 },
    { "ENDDATA", 29 },
    { "INVTSREQ", 30 },
    { "EXPIRED", 31 },
    { "RETPAGE", 32 },
    { "RTEFAIL", 33 },
    { "RTESOME", 34 },
    { "TSIOERR", 35 },
    { "MAPFAIL", 36 },
    { "INVERRTERM", 37 },
    { "INVMPSZ", 38 },
    { "IGREQID", 39 },
    { "OVERFLOW", 40 },
    { "INVLDC", 41 },
    { "NOSTG", 42 },
    { "JIDERR", 43 },
    { "QIDERR", 44 },
    { "NOJBUFSP", 45 },
    { "DSSTAT", 46 },
    { "SELNERR", 47 },
    { "FUNCERR", 48 },
    { "UNEXPIN", 49 },
    { "NOPASSBKRD", 50 },
    { "NOPASSBKWR", 51 },
    { "SEGIDERR", 52 },
    { "SYSIDERR", 53 },
    { "ISCINVREQ", 54 },
    { "ENQBUSY", 55 },
    { "ENVDEFERR", 56 },
    { "IGREQCD", 57 },
    { "SESSIONERR", 58 },
    { "SYSBUSY", 59 },
    { "SESSBUSY", 60 },
    { "NOTALLOC", 61 },
    { "CBIDERR", 62 },
    { "INVEXITREQ", 63 },
    { "INVPARTNSET", 64 },
    { "INVPARTN", 65 },
    { "PARTNFAIL", 66 },
    { "USERIDERR", 69 },
    { "NOTAUTH", 70 },
    { "VOLIDERR", 71 },
    { "SUPPRESSED", 72 },
    { "RESIDERR", 75 },
    { "NOSPOOL", 80 },
    { "TERMERR", 81 },
    { "ROLLEDBACK", 82 },
    { "END", 83 },
    { "DISABLED", 84 },
    { "ALLOCERR", 85 },
    { "STRELERR", 86 },
    { "OPENERR", 87 },
    { "SPOLBUSY", 88 },
    { "SPOLERR", 89 },
    { "NODEIDERR", 90 },
    { "TASKIDERR", 91 },
    { "TCIDERR", 92 },
    { "DSNNOTFOUND", 93 },
    { "LOADING", 94 },
    { "MODELIDERR", 95 },
    { "OUTDESCRERR", 96 },
    { "PARTNERIDERR", 97 },
    { "PROFILEIDERR", 98 },
    { "NETNAMEIDERR", 99 },
    { "LOCKED", 100 },
    { "RECORDBUSY", 101 },
    { "UOWNOTFOUND", 102 },
    { "UOWLNOTFOUND", 103 },
    { "LINKABEND", 104 },
    { "CHANGED", 105 },
    { "PROCESSBUSY", 106 },
    { "ACTIVITYBUSY", 107 },
    { "PROCESSERR", 108 },
    { "ACTIVITYERR", 109 },
    { "CONTAINERERR", 110 },
    { "EVENTERR", 111 },
    { "TOKENERR", 112 },
    { "NOTFINISHED", 113 },
    { "POOLERR", 114 },
    { "TIMERERR", 115 },
    { "SYMBOLERR", 116 },
    { "TEMPLATERR", 117 },
    { "NOTSUPERUSER", 118 },
    { "CSDERR", 119 },
    { "DUPRES", 120 },
    { "RESUNAVAIL", 121 },
    { "CHANNELERR", 122 },
    { "CCSIDERR", 123 },
    { "TIMEDOUT", 124 },
    { "CODEPAGEERR", 125 },
    { "INCOMPLETE", 126 },
    { "APPNOTFOUND", 127 },
    { "BUSY", 128 },
};

const std::unordered_map<std::string_view, int> DFHVALUE_operands = {
    { "ACQUIRED", 69 },
    { "ACQUIRING", 71 },
    { "ACTIVE", 181 },
    { "ADD", 291 },
    { "ADDABLE", 41 },
    { "ADVANCE", 265 },
    { "ALLCONN", 169 },
    { "ALLOCATED", 81 },
    { "ALLQUERY", 431 },
    { "ALTERABLE", 52 },
    { "ALTERNATE", 197 },
    { "ALTPRTCOPY", 446 },
    { "ANY", 158 },
    { "APLKYBD", 391 },
    { "APLTEXT", 393 },
    { "APPC", 124 },
    { "APPCPARALLEL", 374 },
    { "APPCSINGLE", 373 },
    { "ASATCL", 224 },
    { "ASCII7", 616 },
    { "ASCII8", 617 },
    { "ASSEMBLER", 150 },
    { "ATI", 75 },
    { "ATTENTION", 524 },
    { "AUDALARM", 395 },
    { "AUTOACTIVE", 630 },
    { "AUTOARCH", 262 },
    { "AUTOCONN", 170 },
    { "AUTOINACTIVE", 631 },
    { "AUTOPAGEABLE", 80 },
    { "AUXILIARY", 247 },
    { "AUXPAUSE", 313 },
    { "AUXSTART", 312 },
    { "AUXSTOP", 314 },
    { "BACKOUT", 192 },
    { "BACKTRANS", 397 },
    { "BASE", 10 },
    { "BATCHLU", 191 },
    { "BDAM", 2 },
    { "BELOW", 159 },
    { "BGAM", 63 },
    { "BIPROG", 160 },
    { "BISYNCH", 128 },
    { "BIT", 1600 },
    { "BLK", 47 },
    { "BLOCKED", 16 },
    { "BROWSABLE", 39 },
    { "BSAM", 61 },
    { "BTAM_ES", 62 },
    { "BUSY", 612 },
    { "C", 149 },
    { "CANCELLED", 624 },
    { "CDRDLPRT", 24 },
    { "CEDF", 370 },
    { "CICSDATAKEY", 379 },
    { "CICSEXECKEY", 381 },
    { "CICSSECURITY", 195 },
    { "CICSTABLE", 101 },
    { "CHAR", 1601 },
    { "CKOPEN", 1055 },
    { "CLEAR", 640 },
    { "CLOSED", 19 },
    { "CLOSEFAILED", 349 },
    { "CLOSELEAVE", 261 },
    { "CLOSEREQUEST", 22 },
    { "CLOSING", 21 },
    { "CMDPROT", 673 },
    { "CMDSECEXT", 207 },
    { "CMDSECNO", 205 },
    { "CMDSECYES", 206 },
    { "COBOL", 151 },
    { "COBOLII", 375 },
    { "COBOLIT", 1507 },
    { "COLDACQ", 72 },
    { "COLDQUERY", 433 },
    { "COLDSTART", 266 },
    { "COLOR", 399 },
    { "COMMIT", 208 },
    { "CONFFREE", 82 },
    { "CONFRECEIVE", 83 },
    { "CONFSEND", 84 },
    { "CONSOLE", 66 },
    { "CONTNLU", 189 },
    { "CONTROLSHUT", 623 },
    { "COPY", 401 },
    { "CPP", 624 },
    { "CREATE", 67 },
    { "CTLGALL", 632 },
    { "CTLGMODIFY", 633 },
    { "CTLGNONE", 634 },
    { "CTRLABLE", 56 },
    { "CURRENT", 260 },
    { "DB2", 623 },
    { "DEC", 46 },
    { "DEFAULT", 198 },
    { "DELAY", 637 },
    { "DELETABLE", 43 },
    { "DEST", 235 },
    { "DISABLED", 24 },
    { "DISABLING", 25 },
    { "DISCREQ", 444 },
    { "DISK1", 252 },
    { "DISK2", 253 },
    { "DISK2PAUSE", 254 },
    { "DISPATCHABLE", 228 },
    { "DPLSUBSET", 383 },
    { "DS3270", 615 },
    { "DUALCASE", 403 },
    { "DYNAMIC", 178 },
    { "EMERGENCY", 268 },
    { "EMPTY", 210 },
    { "EMPTYREQ", 31 },
    { "ENABLED", 23 },
    { "ESDS", 5 },
    { "EVENT", 334 },
    { "EXCEPT", 332 },
    { "EXCTL", 48 },
    { "EXITTRACE", 362 },
    { "EXTENDEDDS", 405 },
    { "EXTRA", 221 },
    { "EXTSECURITY", 194 },
    { "FAILEDBKOUT", 357 },
    { "FAILINGBKOUT", 358 },
    { "FCLOSE", 273 },
    { "FINALQUIESCE", 183 },
    { "FINPUT", 270 },
    { "FIRSTINIT", 625 },
    { "FIRSTQUIESCE", 182 },
    { "FIXED", 12 },
    { "FMH", 502 },
    { "FMHPARM", 385 },
    { "FOPEN", 272 },
    { "FORCE", 342 },
    { "FORCECLOSE", 351 },
    { "FORCECLOSING", 353 },
    { "FORCEPURGE", 237 },
    { "FORMFEED", 407 },
    { "FOUTPUT", 271 },
    { "FREE", 85 },
    { "FREEING", 94 },
    { "FULL", 212 },
    { "FULLAPI", 384 },
    { "FWDRECOVABLE", 354 },
    { "GENERIC", 651 },
    { "GOINGOUT", 172 },
    { "GFTSTART", 317 },
    { "GFTSTOP", 318 },
    { "HARDCOPY", 32 },
    { "HEX", 45 },
    { "HFORM", 409 },
    { "HILIGHT", 413 },
    { "HOLD", 163 },
    { "IBMCOBOL", 375 },
    { "IGNORE", 1 },
    { "IMMCLOSE", 350 },
    { "IMMCLOSING", 352 },
    { "INACTIVE", 378 },
    { "INDIRECT", 122 },
    { "INDOUBT", 620 },
    { "INFLIGHT", 621 },
    { "INITCOMPLETE", 628 },
    { "INPUT", 226 },
    { "INSERVICE", 73 },
    { "INSTART", 1502 },
    { "INSTOP", 1503 },
    { "INTACTLU", 190 },
    { "INTERNAL", 1058 },
    { "INTRA", 222 },
    { "INTSTART", 310 },
    { "INTSTOP", 311 },
    { "INVALID", 359 },
    { "IPIC", 805 },
    { "IRC", 121 },
    { "ISCMMCONV", 209 },
    { "ISOLATE", 658 },
    { "JAVA", 625 },
    { "KATAKANA", 415 },
    { "KEYED", 8 },
    { "KSDS", 6 },
    { "LE370", 377 },
    { "LIGHTPEN", 417 },
    { "LOG", 54 },
    { "LOGICAL", 216 },
    { "LPA", 165 },
    { "LU61", 125 },
    { "LUCMODGRP", 210 },
    { "LUCSESS", 211 },
    { "LUTYPE4", 193 },
    { "LUTYPE6", 192 },
    { "MAGTAPE", 20 },
    { "MAIN", 248 },
    { "MAP", 155 },
    { "MAPSET", 155 },
    { "MCHCTL", 241 },
    { "MODEL", 370 },
    { "MSRCONTROL", 419 },
    { "NEW", 28 },
    { "NEWCOPY", 167 },
    { "NOALTPRTCOPY", 447 },
    { "NOAPLKYBD", 392 },
    { "NOAPLTEXT", 394 },
    { "NOATI", 76 },
    { "NOAUDALARM", 396 },
    { "NOAUTOARCH", 263 },
    { "NOBACKTRANS", 398 },
    { "NOCEDF", 371 },
    { "NOCLEAR", 641 },
    { "NOCMDPROT", 674 },
    { "NOCOLOR", 400 },
    { "NOCOPY", 402 },
    { "NOCREATE", 68 },
    { "NOCTL", 223 },
    { "NODISCREQ", 445 },
    { "NODUALCASE", 404 },
    { "NOEMPTYREQ", 32 },
    { "NOEVENT", 335 },
    { "NOEXCEPT", 333 },
    { "NOEXCTL", 49 },
    { "NOEXITTRACE", 363 },
    { "NOEXTENDEDDS", 406 },
    { "NOFMH", 503 },
    { "NOFMHPARM", 386 },
    { "NOFORMFEED", 408 },
    { "NOHFORM", 410 },
    { "NOHILIGHT", 414 },
    { "NOHOLD", 164 },
    { "NOISOLATE", 657 },
    { "NOKATAKANA", 416 },
    { "NOLIGHTPEN", 418 },
    { "NOLOG", 55 },
    { "NOMSRCONTROL", 420 },
    { "NONAUTOCONN", 171 },
    { "NOOBFORMAT", 422 },
    { "NOOBOPERID", 388 },
    { "NOOUTLINE", 424 },
    { "NOPARTITIONS", 426 },
    { "NOPERF", 331 },
    { "NOPRESETSEC", 243 },
    { "NOPRINTADAPT", 428 },
    { "NOPROGSYMBOL", 430 },
    { "NOPRTCOPY", 449 },
    { "NOQUERY", 432 },
    { "NOREENTPROT", 681 },
    { "NORELREQ", 443 },
    { "NORMALBKOUT", 356 },
    { "NOSHUTDOWN", 289 },
    { "NOSOSI", 435 },
    { "NOSWITCH", 285 },
    { "NOSYSDUMP", 185 },
    { "NOTADDABLE", 42 },
    { "NOTALTERABLE", 53 },
    { "NOTAPPLIC", 1 },
    { "NOTCTRLABLE", 57 },
    { "NOTEXTKYBD", 437 },
    { "NOTEXTPRINT", 439 },
    { "NOTBROWSABLE", 40 },
    { "NOTBUSY", 613 },
    { "NOTDELETABLE", 44 },
    { "NOTEMPTY", 211 },
    { "NOTERMINAL", 214 },
    { "NOTFWDRCVBLE", 361 },
    { "NOTKEYED", 9 },
    { "NOTLPA", 166 },
    { "NOTPENDING", 127 },
    { "NOTPURGEABLE", 161 },
    { "NOTRANDUMP", 187 },
    { "NOTREADABLE", 36 },
    { "NOTREADY", 259 },
    { "NOTRECOVABLE", 30 },
    { "NOTREQUIRED", 667 },
    { "NOTSOS", 669 },
    { "NOTTABLE", 100 },
    { "NOTINIT", 376 },
    { "NOTTI", 78 },
    { "NOTUPDATABLE", 38 },
    { "NOUCTRAN", 451 },
    { "NOVALIDATION", 441 },
    { "NOVFORM", 412 },
    { "NOWAIT", 341 },
    { "NOZCPTRACE", 365 },
    { "OBFORMAT", 421 },
    { "OBOPERID", 387 },
    { "OBTAINING", 96 },
    { "OFF", 200 },
    { "OK", 274 },
    { "OLD", 26 },
    { "OLDCOPY", 162 },
    { "ON", 201 },
    { "OPEN", 18 },
    { "OPENAPI", 1053 },
    { "OPENING", 20 },
    { "OPENINPUT", 256 },
    { "OPENOUTPUT", 257 },
    { "OUTLINE", 423 },
    { "OUTPUT", 227 },
    { "OUTSERVICE", 74 },
    { "PAGEABLE", 79 },
    { "PARTITIONS", 425 },
    { "PARTITIONSET", 156 },
    { "PATH", 11 },
    { "PENDFREE", 86 },
    { "PENDING", 126 },
    { "PENDRECEIVE", 87 },
    { "PERF", 330 },
    { "PHASEIN", 168 },
    { "PHYSICAL", 215 },
    { "PL1", 152 },
    { "POST", 636 },
    { "PRESETSEC", 242 },
    { "PRIMARY", 110 },
    { "PRINTADAPT", 427 },
    { "PRIVATE", 174 },
    { "PROGRAM", 154 },
    { "PROGSYMBOL", 429 },
    { "PRTCOPY", 448 },
    { "PURGE", 236 },
    { "PURGEABLE", 160 },
    { "QR", 1057 },
    { "READABLE", 35 },
    { "READBACK", 209 },
    { "READONLY", 275 },
    { "READY", 258 },
    { "RECEIVE", 88 },
    { "RECOVERABLE", 29 },
    { "REENTPROT", 680 },
    { "RELEASED", 70 },
    { "RELEASING", 549 },
    { "RELREQ", 442 },
    { "REMOTE", 4 },
    { "REMOVE", 276 },
    { "REQUIRED", 666 },
    { "RESSECEXT", 204 },
    { "RESSECNO", 202 },
    { "RESSECYES", 203 },
    { "RESSYS", 208 },
    { "REVERTED", 264 },
    { "RFC3339", 647 },
    { "ROLLBACK", 89 },
    { "RPC", 1500 },
    { "RRDS", 7 },
    { "RUNNING", 229 },
    { "SCS", 614 },
    { "SDLC", 176 },
    { "SECONDINIT", 626 },
    { "SEND", 90 },
    { "SEQDISK", 18 },
    { "SESSION", 372 },
    { "SFS", 3 },
    { "SHARE", 27 },
    { "SHARED", 173 },
    { "SHUTDISABLED", 645 },
    { "SHUTENABLED", 644 },
    { "SHUTDOWN", 288 },
    { "SIGNEDOFF", 245 },
    { "SIGNEDON", 244 },
    { "SINGLEOFF", 324 },
    { "SINGLEON", 323 },
    { "SMF", 255 },
    { "SOS", 668 },
    { "SOSABOVE", 683 },
    { "SOSBELOW", 682 },
    { "SOSI", 434 },
    { "SPECIFIC", 652 },
    { "SPECTRACE", 177 },
    { "SPRSTRACE", 175 },
    { "SQL", 623 },
    { "STANTRACE", 176 },
    { "START", 635 },
    { "STARTED", 609 },
    { "STARTUP", 180 },
    { "STATIC", 179 },
    { "STOPPED", 610 },
    { "SURROGATE", 371 },
    { "SUSPENDED", 231 },
    { "SWITCH", 188 },
    { "SWITCHALL", 287 },
    { "SWITCHING", 225 },
    { "SWITCHNEXT", 286 },
    { "SYNCFREE", 91 },
    { "SYNCRECEIVE", 92 },
    { "SYNCSEND", 93 },
    { "SYSDUMP", 184 },
    { "SYSTEM", 643 },
    { "SYSTEMOFF", 320 },
    { "SYSTEMON", 319 },
    { "SYSTEM3", 161 },
    { "SYSTEM7", 2 },
    { "SYS370", 164 },
    { "SYS7BSCA", 166 },
    { "TAKEOVER", 111 },
    { "TAPE1", 250 },
    { "TAPE2", 251 },
    { "TASK", 233 },
    { "TCAM", 64 },
    { "TCAMSNA", 65 },
    { "TCEXITALL", 366 },
    { "TCEXITALLOFF", 369 },
    { "TCEXITNONE", 368 },
    { "TCEXITSYSTEM", 367 },
    { "TCONSOLE", 8 },
    { "TCPIP", 802 },
    { "TELETYPE", 34 },
    { "TERM", 234 },
    { "TERMINAL", 213 },
    { "TERMSTATUS", 606 },
    { "TEXTKYBD", 436 },
    { "TEXTPRINT", 438 },
    { "THIRDINIT", 627 },
    { "THREADSAFE", 1051 },
    { "TRANDUMP", 186 },
    { "TRANIDONLY", 452 },
    { "TTCAM", 80 },
    { "TTI", 77 },
    { "TWX33_35", 33 },
    { "T1050", 36 },
    { "T1053", 74 },
    { "T2260L", 65 },
    { "T2260R", 72 },
    { "T2265", 76 },
    { "T2740", 40 },
    { "T2741BCD", 43 },
    { "T2741COR", 42 },
    { "T2772", 130 },
    { "T2780", 132 },
    { "T2980", 134 },
    { "T3275R", 146 },
    { "T3277L", 153 },
    { "T3277R", 145 },
    { "T3284L", 155 },
    { "T3284R", 147 },
    { "T3286L", 156 },
    { "T3286R", 148 },
    { "T3600BI", 138 },
    { "T3601", 177 },
    { "T3614", 178 },
    { "T3650ATT", 186 },
    { "T3650HOST", 185 },
    { "T3650PIPE", 184 },
    { "T3650USER", 187 },
    { "T3735", 136 },
    { "T3740", 137 },
    { "T3780", 133 },
    { "T3790", 180 },
    { "T3790SCSP", 182 },
    { "T3790UP", 181 },
    { "T7770", 1 },
    { "UCTRAN", 450 },
    { "UKOPEN", 1056 },
    { "UNBLOCKED", 17 },
    { "UNDEFINED", 14 },
    { "UNDETERMINED", 355 },
    { "UNENABLED", 33 },
    { "UNENABLING", 34 },
    { "UPDATABLE", 37 },
    { "USER", 642 },
    { "USERDATAKEY", 380 },
    { "USEREXECKEY", 382 },
    { "USEROFF", 322 },
    { "USERON", 321 },
    { "USERTABLE", 102 },
    { "VALID", 360 },
    { "VALIDATION", 440 },
    { "VARIABLE", 13 },
    { "VFORM", 411 },
    { "VIDEOTERM", 64 },
    { "VSAM", 3 },
    { "VTAM", 60 },
    { "WAIT", 340 },
    { "WAITFORGET", 622 },
    { "WARMSTART", 267 },
    { "XM", 123 },
    { "XNOTDONE", 144 },
    { "XOK", 143 },
    { "ZCPTRACE", 364 },
};

// emulates limited variant of alternative operand parser and performs DFHRESP/DFHVALUE substitutions
// recognizes L' attribute, '...' strings and skips end of line comments
template<typename It>
class mini_parser
{
    std::string m_substituted_operands;
    std::match_results<It> m_matches;

    enum class symbol_type : unsigned char
    {
        normal,
        blank,
        apostrophe,
        comma,
        operator_symbol,
    };

    static constexpr std::array symbols = []() {
        std::array<symbol_type, std::numeric_limits<unsigned char>::max() + 1> r {};

        r[(unsigned char)' '] = symbol_type::blank;
        r[(unsigned char)'\''] = symbol_type::apostrophe;
        r[(unsigned char)','] = symbol_type::comma;

        r[(unsigned char)'*'] = symbol_type::operator_symbol;
        r[(unsigned char)'.'] = symbol_type::operator_symbol;
        r[(unsigned char)'-'] = symbol_type::operator_symbol;
        r[(unsigned char)'+'] = symbol_type::operator_symbol;
        r[(unsigned char)'='] = symbol_type::operator_symbol;
        r[(unsigned char)'<'] = symbol_type::operator_symbol;
        r[(unsigned char)'>'] = symbol_type::operator_symbol;
        r[(unsigned char)'('] = symbol_type::operator_symbol;
        r[(unsigned char)')'] = symbol_type::operator_symbol;
        r[(unsigned char)'/'] = symbol_type::operator_symbol;
        r[(unsigned char)'&'] = symbol_type::operator_symbol;
        r[(unsigned char)'|'] = symbol_type::operator_symbol;

        return r;
    }();

    std::optional<std::string> try_dfh_consume(
        It& b, const It& e, const words_to_consume& wtc, const std::unordered_map<std::string_view, int>& value_map)
    {
        const auto reverter = [backup = b, &b]() {
            b = backup;
            return std::nullopt;
        };

        static const auto dfh_value_end_separators = [](const It& b, const It& e) {
            return (b == e || (*b != ' ' && *b != ')')) ? 0 : 1;
        };

        if (!consume_words_advance_to_next<It>(b, e, wtc, space_separator<It>))
            return reverter();

        if (b == e || *b++ != '(')
            return reverter();
        trim_left<It>(b, e, space_separator<It>);

        auto val = next_continuous_sequence<It>(b, e, dfh_value_end_separators);

        trim_left<It>(b, e, space_separator<It>);
        if (b == e || *b++ != ')')
            return reverter();

        if (!val)
            return std::string();

        auto map_it = value_map.find(context::to_upper_copy(*val));
        if (map_it == value_map.end())
            return reverter();

        return std::to_string(map_it->second);
    }

public:
    const std::string& operands() const& { return m_substituted_operands; }
    std::string operands() && { return std::move(m_substituted_operands); }

    class parse_and_substitute_result
    {
        std::variant<size_t, std::string_view> m_value;

    public:
        explicit parse_and_substitute_result(size_t substitutions_performed)
            : m_value(substitutions_performed)
        {}
        explicit parse_and_substitute_result(std::string_view var_name)
            : m_value(var_name)
        {}

        bool error() const { return std::holds_alternative<std::string_view>(m_value); }

        std::string_view error_variable_name() const { return std::get<std::string_view>(m_value); }

        size_t substitutions_performed() const { return std::get<size_t>(m_value); }
    };

    parse_and_substitute_result parse_and_substitute(It b, const It& e)
    {
        m_substituted_operands.clear();
        size_t valid_dfh = 0;

        bool next_last_attribute = false;
        bool next_new_token = true;
        while (b != e)
        {
            const bool last_attribute = std::exchange(next_last_attribute, false);
            const bool new_token = std::exchange(next_new_token, false);
            const char c = *b;
            const symbol_type s = symbols[(unsigned char)c];

            switch (s)
            {
                case symbol_type::normal:
                    if (!new_token)
                        break;
                    if (c == 'L' || c == 'l')
                    {
                        if (auto n = std::next(b); n != e && *n == '\'')
                        {
                            m_substituted_operands.push_back(c);
                            m_substituted_operands.push_back('\'');
                            ++b;
                            ++b;
                            next_last_attribute = true;
                            next_new_token = true;
                            continue;
                        }
                    }
                    else if (!last_attribute && (c == 'D' || c == 'd'))
                    {
                        static const words_to_consume dfhresp_wtc({ "DFHRESP" }, false, true);
                        static const words_to_consume dfhvalue_wtc({ "DFHVALUE" }, false, true);

                        // check for DFHRESP/DFHVALUE expression
                        auto expr_start = b;
                        auto val = try_dfh_consume(b, e, dfhresp_wtc, DFHRESP_operands);
                        if (!val.has_value())
                            val = try_dfh_consume(b, e, dfhvalue_wtc, DFHVALUE_operands);

                        if (val.has_value())
                        {
                            if (!val->empty())
                                m_substituted_operands.append("=F'").append(*val).append("'");
                            else if (std::advance(expr_start, 3);
                                     *expr_start == 'R' || *expr_start == 'r') // indicate NULL argument error
                                return parse_and_substitute_result("DFHRESP");
                            else
                                return parse_and_substitute_result("DFHVALUE");

                            ++valid_dfh;
                            continue;
                        }
                    }
                    break;

                case symbol_type::blank:
                    // everything that follows is a comment
                    goto done;

                case symbol_type::apostrophe:
                    // read string literal
                    next_new_token = true;
                    do
                    {
                        m_substituted_operands.push_back(*b);
                        ++b;
                        if (b == e)
                            goto done;
                    } while (*b != '\'');
                    break;

                case symbol_type::comma:
                    next_new_token = true;
                    if (auto n = std::next(b); n != e && *n == ' ')
                    {
                        // skips comment at the end of the line
                        m_substituted_operands.push_back(c);
                        auto skip_line = b;
                        while (skip_line != e && same_line(b, skip_line))
                            ++skip_line;
                        b = skip_line;
                        continue;
                    }
                    break;

                case symbol_type::operator_symbol:
                    next_new_token = true;
                    break;

                default:
                    assert(false);
                    break;
            }
            m_substituted_operands.push_back(c);
            ++b;
        }

    done:
        return parse_and_substitute_result(valid_dfh);
    }
};

class cics_preprocessor final : public preprocessor
{
    lexing::logical_line m_logical_line;
    library_fetcher m_libs;
    diagnostic_op_consumer* m_diags = nullptr;
    std::vector<document_line> m_result;
    cics_preprocessor_options m_options;

    bool m_end_seen = false;
    bool m_global_macro_called = false;
    bool m_pending_prolog = false;
    bool m_pending_dfheistg_prolog = false;
    std::string_view m_pending_dfh_null_error;

    std::match_results<std::string_view::iterator> m_matches_sv;
    std::match_results<lexing::logical_line::const_iterator> m_matches_ll;

    mini_parser<lexing::logical_line::const_iterator> m_mini_parser;

    semantics::source_info_processor& m_src_proc;

public:
    cics_preprocessor(const cics_preprocessor_options& options,
        library_fetcher libs,
        diagnostic_op_consumer* diags,
        semantics::source_info_processor& src_proc)
        : m_libs(std::move(libs))
        , m_diags(diags)
        , m_options(options)
        , m_src_proc(src_proc)
    {}

    void inject_no_end_warning()
    {
        m_result.emplace_back(replaced_line { "*DFH7041I W  NO END CARD FOUND - COPYBOOK ASSUMED.\n" });
        m_result.emplace_back(replaced_line { "         DFHEIMSG 4\n" });
    }

    void inject_DFHEIGBL(bool rsect)
    {
        if (rsect)
        {
            if (m_options.leasm)
                m_result.emplace_back(replaced_line { "         DFHEIGBL ,,RS,LE          INSERTED BY TRANSLATOR\n" });
            else
                m_result.emplace_back(replaced_line { "         DFHEIGBL ,,RS,NOLE        INSERTED BY TRANSLATOR\n" });
        }
        else
        {
            if (m_options.leasm)
                m_result.emplace_back(replaced_line { "         DFHEIGBL ,,,LE            INSERTED BY TRANSLATOR\n" });
            else
                m_result.emplace_back(replaced_line { "         DFHEIGBL ,,,NOLE          INSERTED BY TRANSLATOR\n" });
        }
    }

    void inject_prolog()
    {
        m_result.emplace_back(replaced_line { "         DFHEIENT                  INSERTED BY TRANSLATOR\n" });
    }
    void inject_dfh_null_error(std::string_view variable)
    {
        m_result.emplace_back(
            replaced_line { concat("*DFH7218I S  SUB-OPERAND(S) OF '", variable, "' CANNOT BE NULL. COMMAND NOT\n") });
        m_result.emplace_back(replaced_line { "*            TRANSLATED.\n" });
        m_result.emplace_back(replaced_line { "         DFHEIMSG 12\n" });
    }
    void inject_end_code()
    {
        if (m_options.epilog)
            m_result.emplace_back(replaced_line { "         DFHEIRET                  INSERTED BY TRANSLATOR\n" });
        if (m_options.prolog)
        {
            m_result.emplace_back(replaced_line { "         DFHEISTG                  INSERTED BY TRANSLATOR\n" });
            m_result.emplace_back(replaced_line { "         DFHEIEND                  INSERTED BY TRANSLATOR\n" });
        }
    }
    void inject_DFHEISTG()
    {
        m_result.emplace_back(replaced_line { "         DFHEISTG                  INSERTED BY TRANSLATOR\n" });
    }

    bool try_asm_xopts(std::string_view input, size_t lineno)
    {
        static constexpr auto sv_icase_compare = [](const std::string_view& sv_a, const std::string_view& sv_b) {
            if (sv_a.size() != sv_b.size())
                return false;

            return std::equal(
                sv_a.begin(), sv_a.end(), sv_b.begin(), [](auto a, auto b) { return tolower(a) == tolower(b); });
        };

        if (!sv_icase_compare(input.substr(0, 5), "*ASM "))
            return false;

        auto [line, _] = lexing::extract_line(input);
        if (m_diags && line.size() > lexing::default_ictl.end && line[lexing::default_ictl.end] != ' ')
            m_diags->add_diagnostic(diagnostic_op::warn_CIC001(range(position(lineno, 0))));

        line = line.substr(5, lexing::default_ictl.end - 5);

        if (auto keyword = utils::next_continuous_sequence(line, "(\'"); sv_icase_compare(keyword, "XOPTS")
            || sv_icase_compare(keyword, "XOPT") || sv_icase_compare(keyword, "CICS"))
            line.remove_prefix(keyword.length() + 1);
        else
            return false;

        std::vector<std::string_view> words;
        while (!line.empty() && (line.front() != '\'' && line.front() != ')'))
        {
            words.emplace_back(utils::next_continuous_sequence(line, " ,)\'"));
            line.remove_prefix(words.back().length());
            auto tmp = line.find_first_not_of(" ,");
            if (tmp != std::string_view::npos)
                line.remove_prefix(tmp);
        }

        if (line.empty() || (line.front() != '\'' && line.front() != ')'))
            return false;

        std::for_each(words.begin(), words.end(), [&options = this->m_options](const auto& w) {
            static const std::unordered_map<std::string_view, std::pair<bool cics_preprocessor_options::*, bool>>
                preproc_opts {
                    { "PROLOG", { &cics_preprocessor_options::prolog, true } },
                    { "NOPROLOG", { &cics_preprocessor_options::prolog, false } },
                    { "EPILOG", { &cics_preprocessor_options::epilog, true } },
                    { "NOEPILOG", { &cics_preprocessor_options::epilog, false } },
                    { "LEASM", { &cics_preprocessor_options::leasm, true } },
                    { "NOLEASM", { &cics_preprocessor_options::leasm, false } },
                };

            if (w.length() != 0)
            {
                if (auto o = preproc_opts.find(w); o != preproc_opts.end())
                    (options.*o->second.first) = o->second.second;
            }
        });

        return true;
    }

    bool process_asm_statement(std::string_view type, std::string_view sect_name)
    {
        switch (type.front())
        {
            case 'D':
                if (!std::exchange(m_global_macro_called, true))
                    inject_DFHEIGBL(false);
                if (type.starts_with("DFHE"))
                    return false;
                // DSECT otherwise
                if (sect_name != "DFHEISTG")
                    return false;
                m_pending_dfheistg_prolog = m_options.prolog;
                break;

            case 'S':
            case 'C':
                if (!std::exchange(m_global_macro_called, true))
                    inject_DFHEIGBL(false);
                m_pending_prolog = m_options.prolog;
                break;

            case 'R':
                m_global_macro_called = true;
                inject_DFHEIGBL(true);
                m_pending_prolog = m_options.prolog;
                break;

            case 'E':
                m_end_seen = true;
                inject_end_code();
                break;

            default:
                assert(false);
                break;
        }
        return true;
    }

    static constexpr const lexing::logical_line_extractor_args cics_extract { 1, 71, 2, false, false };

    static constexpr size_t valid_cols = 1 + lexing::default_ictl.end - (lexing::default_ictl.begin - 1);
    static auto create_line_preview(std::string_view input)
    {
        return utils::utf8_substr(lexing::extract_line(input).first, lexing::default_ictl.begin - 1, valid_cols);
    }

    static bool is_ignored_line(std::string_view line, size_t line_len_chars)
    {
        if (line.empty() || line.front() == '*' || line.starts_with(".*"))
            return true;

        // apparently lines full of characters are ignored
        if (line_len_chars == valid_cols && line.find(' ') == std::string_view::npos)
            return true;

        return false;
    }

    bool process_line_of_interest(std::string_view line)
    {
        static const std::vector<words_to_consume> interesting_words({
            words_to_consume({ "START" }, false, true),
            words_to_consume({ "CSECT" }, false, true),
            words_to_consume({ "RSECT" }, false, true),
            words_to_consume({ "DSECT" }, false, true),
            words_to_consume({ "DFHEIENT" }, false, true),
            words_to_consume({ "DFHEISTG" }, false, true),
            words_to_consume({ "END" }, false, true),
        });

        auto section_name = utils::next_continuous_sequence(line);
        line.remove_prefix(section_name.length());
        utils::trim_left(line);
        auto wtc_it = std::find_if(interesting_words.begin(), interesting_words.end(), [&line](const auto& wtc) {
            auto it = line.begin();
            return consume_words_advance_to_next<std::string_view::const_iterator>(
                it, line.end(), wtc, space_separator<std::string_view::const_iterator>);
        });

        if (wtc_it == interesting_words.end())
            return false;

        return process_asm_statement(wtc_it->words_uc.front(), section_name);
    }

    struct label_info
    {
        size_t byte_length;
        size_t char_length;
    };

    void echo_text(const label_info& li)
    {
        // print lines, remove continuation character and label on the first line
        bool first_line = true;
        for (const auto& l : m_logical_line.segments)
        {
            std::string buffer;
            buffer.append(utils::utf8_substr(l.line, 0, cics_extract.end).str);

            if (auto after_cont = utils::utf8_substr(l.line, cics_extract.end + 1).str; !after_cont.empty())
                buffer.append(" ").append(after_cont);

            if (first_line)
                buffer.replace(0, li.byte_length, li.char_length, ' ');

            buffer[0] = '*';
            buffer.append("\n");
            m_result.emplace_back(replaced_line { std::move(buffer) });
            first_line = false;
        }
    }

    static std::string generate_label_fragment(std::string_view label, const label_info& li)
    {
        if (li.char_length <= 8)
            return std::string(label).append(std::string(9 - li.char_length, ' '));
        else
            return std::string(label).append(" DS 0H\n");
    }

    void inject_call(std::string_view label, const label_info& li)
    {
        if (li.char_length <= 8)
            m_result.emplace_back(replaced_line { generate_label_fragment(label, li) + "DFHECALL =X'0E'\n" });
        else
        {
            m_result.emplace_back(replaced_line { generate_label_fragment(label, li) });
            m_result.emplace_back(replaced_line { "         DFHECALL =X'0E'\n" });
        }
        // TODO: generate correct calls
    }

    void process_exec_cics(std::string_view label)
    {
        label_info li {
            label.length(),
            (size_t)std::count_if(label.begin(), label.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; }),
        };
        echo_text(li);
        inject_call(label, li);
    }

    static bool is_command_present(const std::match_results<lexing::logical_line::const_iterator>& matches)
    {
        return matches[3].matched;
    }

    bool try_exec_cics(preprocessor::line_iterator& line_it,
        const preprocessor::line_iterator& line_it_e,
        const std::optional<size_t>& potential_lineno)
    {
        using It = lexing::logical_line::const_iterator;

        line_it = extract_nonempty_logical_line(m_logical_line, line_it, line_it_e, cics_extract);
        bool exec_cics_continuation_error = false;
        if (m_logical_line.continuation_error)
        {
            exec_cics_continuation_error = true;
            // keep 1st line only
            m_logical_line.segments.erase(m_logical_line.segments.begin() + 1, m_logical_line.segments.end());
        }

        stmt_part_details<It> stmt_iterators { stmt_part_details<It>::it_string_tuple(
            m_logical_line.begin(), m_logical_line.end()) };
        auto it = m_logical_line.begin();
        auto it_e = m_logical_line.end();

        auto label = next_continuous_sequence<It>(it, it_e, space_separator<It>);
        stmt_iterators.label = { m_logical_line.begin(), it, std::move(label) };
        trim_left<It>(it, it_e, space_separator<It>);

        auto instr_start = it;

        static const words_to_consume exec_cics_wtc({ "EXEC", "CICS" }, false, false);
        if (!consume_words_advance_to_next<It>(it, it_e, exec_cics_wtc, space_separator<It>))
            return false;

        auto command = next_continuous_sequence<It>(it, it_e, space_separator<It>);
        stmt_iterators.instruction.emplace_back(
            stmt_part_details<It>::it_string_tuple(std::move(instr_start), it, std::move(command)));

        trim_left<It>(it, it_e, space_separator<It>);
        stmt_iterators.operands = stmt_part_details<It>::it_string_tuple(it, it_e);

        const auto diag_adder = [&diags = this->m_diags](diagnostic_op d) {
            if (diags)
                diags->add_diagnostic(d);
        };
        auto lineno = potential_lineno.value_or(0);

        if (stmt_iterators.instruction.front().name && !stmt_iterators.instruction.front().name->empty())
        {
            process_exec_cics(stmt_iterators.label->name.value_or(""));

            if (exec_cics_continuation_error)
            {
                diag_adder(diagnostic_op::warn_CIC001(range(position(lineno, 0))));
                m_result.emplace_back(replaced_line { "*DFH7080I W  CONTINUATION OF EXEC COMMAND IGNORED.\n" });
                m_result.emplace_back(replaced_line { "         DFHEIMSG 4\n" });
            }
        }
        else
        {
            diag_adder(diagnostic_op::warn_CIC003(range(position(lineno, 0))));
            m_result.emplace_back(replaced_line { "*DFH7237I S  INCORRECT SYNTAX AFTER 'EXEC CICS'. COMMAND NOT\n" });
            m_result.emplace_back(replaced_line { "*            TRANSLATED.\n" });
            m_result.emplace_back(replaced_line { "         DFHEIMSG 12\n" });

            stmt_iterators.instruction.front().name = "EXEC CICS";
        }

        if (potential_lineno)
        {
            auto stmt = get_preproc_statement2<semantics::preprocessor_statement_si>(stmt_iterators, lineno, 1);
            do_highlighting(*stmt, m_logical_line, m_src_proc, 1);
            set_statement(std::move(stmt));
        }

        return true;
    }

    auto try_substituting_dfh(const stmt_part_details<lexing::logical_line::const_iterator>& stmt_iterators)
    {
        assert(stmt_iterators.label.has_value() && stmt_iterators.instruction.front().name.has_value()
            && stmt_iterators.operands.has_value());

        auto events = m_mini_parser.parse_and_substitute(stmt_iterators.operands->it_s, stmt_iterators.operands->it_e);
        if (!events.error() && events.substitutions_performed() > 0)
        {
            const auto& label_b = stmt_iterators.label->it_s;
            const auto& label_e = stmt_iterators.label->it_e;

            label_info li {
                (size_t)std::distance(label_b, label_e),
                (size_t)std::count_if(label_b, label_e, [](unsigned char c) { return (c & 0xc0) != 0x80; }),
            };

            echo_text(li);

            std::string text_to_add = std::string(stmt_iterators.instruction.front().name.value());
            if (auto instr_len = utils::utf8_substr(text_to_add).char_count; instr_len < 4)
                text_to_add.append(4 - instr_len, ' ');
            text_to_add.append(1, ' ').append(m_mini_parser.operands());
            text_to_add.insert(0, generate_label_fragment(std::string(label_b, label_e), li));

            std::string_view prefix;
            std::string_view t = text_to_add;

            size_t line_limit = 62;
            while (true)
            {
                auto part = utils::utf8_substr(t, 0, line_limit);
                t.remove_prefix(part.str.size());

                if (t.empty())
                {
                    m_result.emplace_back(replaced_line { concat(prefix, part.str, "\n") });
                    break;
                }
                else
                    m_result.emplace_back(replaced_line { concat(prefix, part.str, "*\n") });

                prefix = "               ";
                line_limit = 56;
            }
        }

        return events;
    }

    bool consume_dfh_values(
        lexing::logical_line::const_iterator& it, const lexing::logical_line::const_iterator& it_e, bool nested)
    {
        using It = lexing::logical_line::const_iterator;

        const auto reverter = [backup = it, &it]() {
            it = backup;
            return false;
        };

        static const words_to_consume dfhresp_wtc({ "DFHRESP" }, false, true);
        static const words_to_consume dfhvalue_wtc({ "DFHVALUE" }, false, true);

        if (consume_words_advance_to_next<It>(it, it_e, dfhresp_wtc, space_separator<It>)
            || consume_words_advance_to_next<It>(it, it_e, dfhvalue_wtc, space_separator<It>))
        {
            if (it == it_e || *it++ != '(')
                return reverter();

            trim_left<It>(it, it_e, space_separator<It>);
            consume_dfh_values(it, it_e, true);
            trim_left<It>(it, it_e, space_separator<It>);

            if (it == it_e || *it++ != ')')
                return reverter();
        }
        else if (nested)
        {
            static const auto space_parenthesis_separator = [](const It& it, const It& it_e) {
                return (it == it_e || (*it != ' ' && *it != ')')) ? 0 : 1;
            };

            skip_past_next_continuous_sequence<It>(it, it_e, space_parenthesis_separator);
            trim_left<It>(it, it_e, space_separator<It>);
        }
        else
            return reverter();

        return true;
    }

    bool skip_past_dfh_values(
        lexing::logical_line::const_iterator& it, const lexing::logical_line::const_iterator& it_e)
    {
        using It = lexing::logical_line::const_iterator;

        static const auto comma_separator = [](const It& it, const It& it_e) {
            return (it == it_e || *it != ',') ? 0 : 1;
        };

        while (skip_past_next_continuous_sequence<It>(it, it_e, comma_separator))
            ;

        trim_left<It>(it, it_e, comma_separator);
        if (it == it_e)
            return false;

        return consume_dfh_values(it, it_e, false);
    }

    bool try_dfh_lookup(preprocessor::line_iterator& line_it,
        const preprocessor::line_iterator& line_it_e,
        const std::optional<size_t>& potential_lineno)
    {
        using It = lexing::logical_line::const_iterator;

        const auto diag_adder = [&diags = this->m_diags](diagnostic_op d) {
            if (diags)
                diags->add_diagnostic(d);
        };

        auto lineno = potential_lineno.value_or(0);
        line_it = extract_nonempty_logical_line(m_logical_line, line_it, line_it_e, lexing::default_ictl);

        if (m_logical_line.continuation_error)
        {
            diag_adder(diagnostic_op::warn_CIC001(range(position(lineno, 0))));
            return false;
        }

        auto it = m_logical_line.begin();
        auto it_e = m_logical_line.end();
        stmt_part_details<It> stmt_iterators { stmt_part_details<It>::it_string_tuple(it, it_e) };

        auto label = next_continuous_sequence<It>(it, it_e, space_separator<It>);
        stmt_iterators.label = { m_logical_line.begin(), it, std::move(label) };
        trim_left<It>(it, it_e, space_separator<It>);

        auto instr_start = it;
        auto instruction = next_continuous_sequence<It>(it, it_e, space_separator<It>);
        stmt_iterators.instruction.emplace_back(
            stmt_part_details<It>::it_string_tuple(std::move(instr_start), it, std::move(instruction)));
        trim_left<It>(it, it_e, space_separator<It>);

        if (it == it_e)
            return false;

        auto operand_start = it;
        if (!skip_past_dfh_values(it, it_e))
            return false;

        stmt_iterators.operands = stmt_part_details<It>::it_string_tuple(std::move(operand_start), it);
        trim_left<It>(it, it_e, space_separator<It>);

        stmt_iterators.remarks = stmt_part_details<It>::it_string_tuple(it, it_e);

        if (potential_lineno)
        {
            auto stmt = get_preproc_statement2<semantics::preprocessor_statement_si>(stmt_iterators, lineno);
            do_highlighting(*stmt, m_logical_line, m_src_proc);
            set_statement(std::move(stmt));
        }

        if (auto r = try_substituting_dfh(stmt_iterators); r.error())
        {
            diag_adder(diagnostic_op::warn_CIC002(range(position(lineno, 0)), r.error_variable_name()));
            m_pending_dfh_null_error = r.error_variable_name();

            return false;
        }
        else
            return r.substitutions_performed() > 0;
    }

    static bool is_process_line(std::string_view s)
    {
        static constexpr const std::string_view PROCESS = "*PROCESS ";
        return s.size() >= PROCESS.size()
            && std::equal(PROCESS.begin(), PROCESS.end(), s.begin(), [](unsigned char l, unsigned char r) {
                   return l == toupper(r);
               });
    }

    void do_general_injections()
    {
        if (std::exchange(m_pending_prolog, false))
            inject_prolog();
        if (std::exchange(m_pending_dfheistg_prolog, false))
            inject_DFHEISTG();
        if (!m_pending_dfh_null_error.empty())
            inject_dfh_null_error(std::exchange(m_pending_dfh_null_error, std::string_view()));
    }

    // Inherited via preprocessor
    document generate_replacement(document doc) override
    {
        reset();
        m_result.clear();
        m_result.reserve(doc.size());

        auto it = doc.begin();
        const auto end = doc.end();

        bool skip_continuation = false;
        bool asm_xopts_allowed = true;
        while (it != end)
        {
            const auto text = it->text();
            if (skip_continuation)
            {
                m_result.emplace_back(*it++);
                skip_continuation = is_continued(text);
                continue;
            }

            do_general_injections();

            const auto lineno = it->lineno(); // TODO: preprocessor chaining

            if (asm_xopts_allowed && is_process_line(text))
            {
                m_result.emplace_back(*it++);
                // ignores continuation
                continue;
            }

            if (asm_xopts_allowed && try_asm_xopts(it->text(), lineno.value_or(0)))
            {
                m_result.emplace_back(*it++);
                // ignores continuation
                continue;
            }

            asm_xopts_allowed = false;

            if (auto [line, line_len_chars, _, __] = create_line_preview(text);
                is_ignored_line(line, line_len_chars) || process_line_of_interest(line))
            {
                m_result.emplace_back(*it++);
                skip_continuation = is_continued(text);
                continue;
            }

            const auto it_backup = it;
            if (try_exec_cics(it, end, lineno))
                continue;

            it = it_backup;
            if (try_dfh_lookup(it, end, lineno))
                continue;

            it = it_backup;

            m_result.emplace_back(*it++);
            skip_continuation = is_continued(text);
        }

        do_general_injections();
        if (!std::exchange(m_end_seen, true) && !asm_xopts_allowed) // actual code encountered
            inject_no_end_warning();

        return document(std::move(m_result));
    }

    cics_preprocessor_options current_options() const { return m_options; }
};

} // namespace

std::unique_ptr<preprocessor> preprocessor::create(const cics_preprocessor_options& options,
    library_fetcher libs,
    diagnostic_op_consumer* diags,
    semantics::source_info_processor& src_proc)
{
    return std::make_unique<cics_preprocessor>(options, std::move(libs), diags, src_proc);
}

namespace test {
cics_preprocessor_options test_cics_current_options(const preprocessor& p)
{
    return static_cast<const cics_preprocessor&>(p).current_options();
}

std::pair<int, std::string> test_cics_miniparser(const std::vector<std::string_view>& list)
{
    lexing::logical_line ll;
    std::transform(list.begin(), list.end(), std::back_inserter(ll.segments), [](std::string_view s) {
        lexing::logical_line_segment lls;
        lls.code = s;
        return lls;
    });

    mini_parser<lexing::logical_line::const_iterator> p;
    std::pair<int, std::string> result;

    auto p_s = p.parse_and_substitute(ll.begin(), ll.end());
    if (p_s.error())
        result.first = -1;
    else
    {
        result.first = (int)p_s.substitutions_performed();
        result.second = std::move(p).operands();
    }

    return result;
}
} // namespace test

} // namespace hlasm_plugin::parser_library::processing
