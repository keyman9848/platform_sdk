/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.eclipse.org/org/documents/epl-v10.php
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.ide.eclipse.adt.internal.refactorings.core;

import com.android.annotations.NonNull;
import com.android.ide.eclipse.adt.internal.project.BaseProjectHelper;

import org.eclipse.core.resources.IProject;
import org.eclipse.jdt.core.IJavaProject;
import org.eclipse.jdt.core.IType;
import org.eclipse.jdt.internal.corext.refactoring.rename.RenameTypeProcessor;
import org.eclipse.ltk.core.refactoring.participants.RenameRefactoring;


@SuppressWarnings({"javadoc", "restriction"})
public class AndroidTypeRenameParticipantTest extends RefactoringTestBase {
    public void testRefactor1() throws Exception {
        renameType(
                TEST_PROJECT,
                "com.example.refactoringtest.MainActivity",
                true /*updateReferences*/,
                "NewActivityName",

                "CHANGES:\n" +
                "-------\n" +
                "* Rename compilation unit 'MainActivity.java' to 'NewActivityName.java'\n" +
                "\n" +
                "* Android Type Rename\n" +
                "\n" +
                "  * AndroidManifest.xml - /testRefactor1/AndroidManifest.xml\n" +
                "    @@ -16 +16\n" +
                "    -             android:name=\"com.example.refactoringtest.MainActivity\"\n" +
                "    +             android:name=\"com.example.refactoringtest.NewActivityName\"");
    }

    public void testRefactor1_noreferences() throws Exception {
        renameType(
                TEST_PROJECT,
                "com.example.refactoringtest.MainActivity",
                false /*updateReferences*/,
                "NewActivityName",

                "CHANGES:\n" +
                "-------\n" +
                "* Rename compilation unit 'MainActivity.java' to 'NewActivityName.java'");
    }

    public void testRefactor2() throws Exception {
        renameType(
                TEST_PROJECT2,
                "com.example.refactoringtest.CustomView1",
                true /*updateReferences*/,
                "NewCustomViewName",

                "CHANGES:\n" +
                "-------\n" +
                "* Rename compilation unit 'CustomView1.java' to 'NewCustomViewName.java'\n" +
                "\n" +
                "* Android Type Rename\n" +
                "\n" +
                "  * customviews.xml - /testRefactor2/res/layout/customviews.xml\n" +
                "    @@ -9 +9\n" +
                "    -     <com.example.refactoringtest.CustomView1\n" +
                "    +     <com.example.refactoringtest.NewCustomViewName\n" +
                "\n" +
                "\n" +
                "  * customviews.xml - /testRefactor2/res/layout-land/customviews.xml\n" +
                "    @@ -9 +9\n" +
                "    -     <com.example.refactoringtest.CustomView1\n" +
                "    +     <com.example.refactoringtest.NewCustomViewName");
    }

    // ---- Test infrastructure ----

    protected void renameType(
            @NonNull Object[] testData,
            @NonNull String typeFqcn,
            boolean updateReferences,
            @NonNull String newName,
            @NonNull String expected) throws Exception {
        IProject project = createProject(testData);
        IJavaProject javaProject = BaseProjectHelper.getJavaProject(project);
        assertNotNull(javaProject);
        IType type = javaProject.findType(typeFqcn);
        assertNotNull(typeFqcn, type);
        assertTrue(typeFqcn, type.exists());
        RenameTypeProcessor processor = new RenameTypeProcessor(type);
        processor.setNewElementName(newName);
        processor.setUpdateQualifiedNames(true);
        processor.setUpdateSimilarDeclarations(false);
        //processor.setMatchStrategy(?);
        //processor.setFilePatterns(patterns);
        processor.setUpdateReferences(updateReferences);
        assertNotNull(processor);

        RenameRefactoring refactoring = new RenameRefactoring(processor);
        checkRefactoring(refactoring, expected);
    }
}
