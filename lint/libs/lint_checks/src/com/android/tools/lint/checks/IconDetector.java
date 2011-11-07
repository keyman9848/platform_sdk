/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.tools.lint.checks;

import static com.android.tools.lint.detector.api.LintConstants.ANDROID_MANIFEST_XML;
import static com.android.tools.lint.detector.api.LintConstants.ANDROID_URI;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_ICON;
import static com.android.tools.lint.detector.api.LintConstants.ATTR_MIN_SDK_VERSION;
import static com.android.tools.lint.detector.api.LintConstants.DOT_9PNG;
import static com.android.tools.lint.detector.api.LintConstants.DOT_GIF;
import static com.android.tools.lint.detector.api.LintConstants.DOT_JPG;
import static com.android.tools.lint.detector.api.LintConstants.DOT_PNG;
import static com.android.tools.lint.detector.api.LintConstants.DOT_XML;
import static com.android.tools.lint.detector.api.LintConstants.DRAWABLE_FOLDER;
import static com.android.tools.lint.detector.api.LintConstants.DRAWABLE_HDPI;
import static com.android.tools.lint.detector.api.LintConstants.DRAWABLE_LDPI;
import static com.android.tools.lint.detector.api.LintConstants.DRAWABLE_MDPI;
import static com.android.tools.lint.detector.api.LintConstants.DRAWABLE_RESOURCE_PREFIX;
import static com.android.tools.lint.detector.api.LintConstants.DRAWABLE_XHDPI;
import static com.android.tools.lint.detector.api.LintConstants.RES_FOLDER;
import static com.android.tools.lint.detector.api.LintConstants.TAG_APPLICATION;
import static com.android.tools.lint.detector.api.LintConstants.TAG_USES_SDK;
import static com.android.tools.lint.detector.api.LintUtils.difference;
import static com.android.tools.lint.detector.api.LintUtils.endsWith;

import com.android.tools.lint.detector.api.Category;
import com.android.tools.lint.detector.api.Context;
import com.android.tools.lint.detector.api.Detector;
import com.android.tools.lint.detector.api.Issue;
import com.android.tools.lint.detector.api.LintUtils;
import com.android.tools.lint.detector.api.Location;
import com.android.tools.lint.detector.api.Scope;
import com.android.tools.lint.detector.api.Severity;
import com.android.tools.lint.detector.api.Speed;

import org.w3c.dom.Element;

import java.awt.Dimension;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.imageio.ImageIO;
import javax.imageio.ImageReader;
import javax.imageio.stream.ImageInputStream;

/**
 * Checks for common icon problems, such as wrong icon sizes, placing icons in the
 * density independent drawable folder, etc.
 */
public class IconDetector extends Detector.XmlDetectorAdapter {

    private static final boolean INCLUDE_LDPI;
    static {
        boolean includeLdpi = false;

        String value = System.getenv("ANDROID_LINT_INCLUDE_LDPI"); //$NON-NLS-1$
        if (value != null) {
            includeLdpi = Boolean.valueOf(value);
        }
        INCLUDE_LDPI = includeLdpi;
    }

    /** Pattern for the expected density folders to be found in the project */
    private static final Pattern DENSITY_PATTERN = Pattern.compile(
            "^drawable-(xhdpi|hdpi|mdpi"                  //$NON-NLS-1$
                + (INCLUDE_LDPI ? "|ldpi" : "") + ")$");  //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$

    /** Pattern for version qualifiers */
    private final static Pattern VERSION_PATTERN = Pattern.compile("^v(\\d+)$");//$NON-NLS-1$

    private static final String[] REQUIRED_DENSITIES = INCLUDE_LDPI
            ? new String[] { DRAWABLE_LDPI, DRAWABLE_MDPI, DRAWABLE_HDPI, DRAWABLE_XHDPI }
            : new String[] { DRAWABLE_MDPI, DRAWABLE_HDPI, DRAWABLE_XHDPI };

    private static final String[] DENSITY_QUALIFIERS =
        new String[] {
            "-ldpi",  //$NON-NLS-1$
            "-mdpi",  //$NON-NLS-1$
            "-hdpi",  //$NON-NLS-1$
            "-xhdpi"  //$NON-NLS-1$
    };

    /** Wrong icon size according to published conventions */
    public static final Issue ICON_EXPECTED_SIZE = Issue.create(
            "IconExpectedSize", //$NON-NLS-1$
            "Ensures that launcher icons, notification icons etc have the correct size",
            "There are predefined sizes (for each density) for launcher icons. You " +
            "should follow these conventions to make sure your icons fit in with the " +
            "overall look of the platform.",
            Category.ICONS,
            5,
            Severity.WARNING,
            IconDetector.class,
            Scope.ALL_RESOURCES_SCOPE)
            // Still some potential false positives:
            .setEnabledByDefault(false)
            .setMoreInfo(
            "http://developer.android.com/guide/practices/ui_guidelines/icon_design_launcher.html#size"); //$NON-NLS-1$

    /** Inconsistent dip size across densities */
    public static final Issue ICON_DIP_SIZE = Issue.create(
            "IconDipSize", //$NON-NLS-1$
            "Ensures that icons across densities provide roughly the same density-independent size",
            "Checks the all icons which are provided in multiple densities, all compute to " +
            "roughly the same density-independent pixel (dip) size. This catches errors where " +
            "images are either placed in the wrong folder, or icons are changed to new sizes " +
            "but some folders are forgotten.",
            Category.ICONS,
            5,
            Severity.WARNING,
            IconDetector.class,
            Scope.ALL_RESOURCES_SCOPE);

    /** Images in res/drawable folder */
    public static final Issue ICON_LOCATION = Issue.create(
            "IconLocation", //$NON-NLS-1$
            "Ensures that images are not defined in the density-independent drawable folder",
            "The res/drawable folder is intended for density-independent graphics such as " +
            "shapes defined in XML. For bitmaps, move it to drawable-mdpi and consider " +
            "providing higher and lower resolution versions in drawable-ldpi, drawable-hdpi " +
            "and drawable-xhdpi. If the icon *really* is density independent (for example " +
            "a solid color) you can place it in drawable-nodpi.",
            Category.ICONS,
            5,
            Severity.WARNING,
            IconDetector.class,
            Scope.ALL_RESOURCES_SCOPE).setMoreInfo(
            "http://developer.android.com/guide/practices/screens_support.html"); //$NON-NLS-1$

    /** Missing density versions of image */
    public static final Issue ICON_DENSITIES = Issue.create(
            "IconDensities", //$NON-NLS-1$
            "Ensures that icons provide custom versions for all supported densities",
            "Icons will look best if a custom version is provided for each of the " +
            "major screen density classes (low, medium, high, extra high). " +
            "This lint check identifies icons which do not have complete coverage " +
            "across the densities.\n" +
            "\n" +
            "Low density is not really used much anymore, so this check ignores " +
            "the ldpi density. To force lint to include it, set the environment " +
            "variable ANDROID_LINT_INCLUDE_LDPI=true. For more information on " +
            "current density usage, see " +
            "http://developer.android.com/resources/dashboard/screens.html",
            Category.ICONS,
            4,
            Severity.WARNING,
            IconDetector.class,
            Scope.ALL_RESOURCES_SCOPE).setMoreInfo(
            "http://developer.android.com/guide/practices/screens_support.html"); //$NON-NLS-1$

    /** Using .gif bitmaps */
    public static final Issue GIF_USAGE = Issue.create(
            "GifUsage", //$NON-NLS-1$
            "Checks for images using the GIF file format which is discouraged",
            "The .gif file format is discouraged. Consider using .png (preferred) " +
            "or .jpg (acceptable) instead.",
            Category.ICONS,
            5,
            Severity.WARNING,
            IconDetector.class,
            Scope.ALL_RESOURCES_SCOPE).setMoreInfo(
            "http://developer.android.com/guide/topics/resources/drawable-resource.html#Bitmap"); //$NON-NLS-1$

    private int mMinSdk;
    private String mApplicationIcon;

    /** Constructs a new accessibility check */
    public IconDetector() {
    }

    @Override
    public Speed getSpeed() {
        return Speed.SLOW;
    }

    @Override
    public void beforeCheckProject(Context context) {
        mMinSdk = -1;
        mApplicationIcon = null;
    }

    @Override
    public void afterCheckProject(Context context) {
        // Make sure no
        File res = new File(context.project.getDir(), RES_FOLDER);
        if (res.isDirectory()) {
            File[] folders = res.listFiles();
            if (folders != null) {
                boolean checkDensities = context.configuration.isEnabled(ICON_DENSITIES);
                boolean checkDipSizes = context.configuration.isEnabled(ICON_DIP_SIZE);

                Map<File, Dimension> pixelSizes = null;
                if (checkDipSizes) {
                    pixelSizes = new HashMap<File, Dimension>();
                }
                Map<File, Set<String>> folderToNames = new HashMap<File, Set<String>>();
                for (File folder : folders) {
                    String folderName = folder.getName();
                    if (folderName.startsWith(DRAWABLE_FOLDER)) {
                        File[] files = folder.listFiles();
                        if (files != null) {
                            checkDrawableDir(context, folder, files, pixelSizes);

                            if (checkDensities && DENSITY_PATTERN.matcher(folderName).matches()) {
                                Set<String> names = new HashSet<String>(files.length);
                                for (File f : files) {
                                    String name = f.getName();
                                    if (!endsWith(name, DOT_XML)) {
                                        names.add(name);
                                    }
                                }
                                folderToNames.put(folder, names);
                            }
                        }
                    }
                }

                if (pixelSizes != null) {
                    assert checkDipSizes;
                    checkDipSizes(context, pixelSizes);
                }

                if (checkDensities && folderToNames.size() > 0) {
                    checkDensities(context, res, folderToNames);
                }
            }
        }
    }

    private void checkDipSizes(Context context, Map<File, Dimension> pixelSizes) {
        // Partition up the files such that I can look at a series by name. This
        // creates a map from filename (such as foo.png) to a list of files
        // providing that icon in various folders: drawable-mdpi/foo.png, drawable-hdpi/foo.png
        // etc.
        Map<String, List<File>> nameToFiles = new HashMap<String, List<File>>();
        for (File file : pixelSizes.keySet()) {
            String name = file.getName();
            List<File> list = nameToFiles.get(name);
            if (list == null) {
                list = new ArrayList<File>();
                nameToFiles.put(name, list);
            }
            list.add(file);
        }

        ArrayList<String> names = new ArrayList<String>(nameToFiles.keySet());
        Collections.sort(names);

        // We have to partition the files further because it's possible for the project
        // to have different configurations for an icon, such as this:
        //   drawable-large-hdpi/foo.png, drawable-large-mdpi/foo.png,
        //   drawable-hdpi/foo.png, drawable-mdpi/foo.png,
        //    drawable-hdpi-v11/foo.png and drawable-mdpi-v11/foo.png.
        // In this case we don't want to compare across categories; we want to
        // ensure that the drawable-large-{density} icons are consistent,
        // that the drawable-{density}-v11 icons are consistent, and that
        // the drawable-{density} icons are consistent.

        // Map from name to list of map from parent folder to list of files
        Map<String, Map<String, List<File>>> configMap =
                new HashMap<String, Map<String,List<File>>>();
        for (Map.Entry<String, List<File>> entry : nameToFiles.entrySet()) {
            String name = entry.getKey();
            List<File> files = entry.getValue();
            for (File file : files) {
                String parentName = file.getParentFile().getName();
                // Strip out the density part
                int index = -1;
                for (String qualifier : DENSITY_QUALIFIERS) {
                    index = parentName.indexOf(qualifier);
                    if (index != -1) {
                        parentName = parentName.substring(0, index)
                                + parentName.substring(index + qualifier.length());
                        break;
                    }
                }
                if (index == -1) {
                    // No relevant qualifier found in the parent directory name,
                    // e.g. it's just "drawable" or something like "drawable-nodpi".
                    continue;
                }

                Map<String, List<File>> folderMap = configMap.get(name);
                if (folderMap == null) {
                    folderMap = new HashMap<String,List<File>>();
                    configMap.put(name, folderMap);
                }
                // Map from name to a map from parent folder to files
                List<File> list = folderMap.get(parentName);
                if (list == null) {
                    list = new ArrayList<File>();
                    folderMap.put(parentName, list);
                }
                list.add(file);
            }
        }

        for (String name : names) {
            //List<File> files = nameToFiles.get(name);
            Map<String, List<File>> configurations = configMap.get(name);
            if (configurations == null) {
                // Nothing in this configuration: probably only found in drawable/ or
                // drawable-nodpi etc directories.
                continue;
            }

            for (Map.Entry<String, List<File>> entry : configurations.entrySet()) {
                List<File> files = entry.getValue();

                // Ensure that all the dip sizes are *roughly* the same
                Map<File, Dimension> dipSizes = new HashMap<File, Dimension>();
                int dipWidthSum = 0; // Incremental computation of average
                int dipHeightSum = 0; // Incremental computation of average
                int count = 0;
                for (File file : files) {
                    float factor = getMdpiScalingFactor(file.getParentFile().getName());
                    if (factor > 0) {
                        Dimension size = pixelSizes.get(file);
                        Dimension dip = new Dimension(
                                Math.round(size.width / factor),
                                Math.round(size.height / factor));
                        dipWidthSum += dip.width;
                        dipHeightSum += dip.height;
                        dipSizes.put(file, dip);
                        count++;
                    }
                }
                if (count == 0) {
                    // Icons in drawable/ and drawable-nodpi/
                    continue;
                }
                int meanWidth = dipWidthSum / count;
                int meanHeight = dipHeightSum / count;

                // Compute standard deviation?
                int squareWidthSum = 0;
                int squareHeightSum = 0;
                for (Dimension size : dipSizes.values()) {
                    squareWidthSum += (size.width - meanWidth) * (size.width - meanWidth);
                    squareHeightSum += (size.height - meanHeight) * (size.height - meanHeight);
                }
                double widthStdDev = Math.sqrt(squareWidthSum / count);
                double heightStdDev = Math.sqrt(squareHeightSum / count);

                if (widthStdDev > meanWidth / 10 || heightStdDev > meanHeight) {
                    Location location = null;
                    StringBuilder sb = new StringBuilder();

                    // Sort entries by decreasing dip size
                    List<Map.Entry<File, Dimension>> entries =
                            new ArrayList<Map.Entry<File,Dimension>>();
                    for (Map.Entry<File, Dimension> entry2 : dipSizes.entrySet()) {
                        entries.add(entry2);
                    }
                    Collections.sort(entries,
                            new Comparator<Map.Entry<File, Dimension>>() {
                        public int compare(Entry<File, Dimension> e1,
                                Entry<File, Dimension> e2) {
                            Dimension d1 = e1.getValue();
                            Dimension d2 = e2.getValue();
                            if (d1.width != d2.width) {
                                return d2.width - d1.width;
                            }

                            return d2.height - d1.height;
                        }
                    });
                    for (Map.Entry<File, Dimension> entry2 : entries) {
                        if (sb.length() > 0) {
                            sb.append(", ");
                        }
                        File file = entry2.getKey();

                        // Chain locations together
                        Location linkedLocation = location;
                        location = new Location(file, null, null);
                        location.setSecondary(linkedLocation);
                        Dimension dip = entry2.getValue();
                        Dimension px = pixelSizes.get(file);
                        String fileName = file.getParentFile().getName() + File.separator
                                + file.getName();
                        sb.append(String.format("%1$s: %2$dx%3$d dp (%4$dx%5$d px)",
                                fileName, dip.width, dip.height, px.width, px.height));
                    }
                    String message = String.format(
                        "The image %1$s varies significantly in its density-independent (dip) " +
                        "size across the various density versions: %2$s",
                            name, sb.toString());
                    context.client.report(context,
                            ICON_DIP_SIZE,
                            location,
                            message,
                            null);
                }
            }
        }
    }

    private void checkDensities(Context context, File res, Map<File, Set<String>> folderToNames) {
        // TODO: Is there a way to look at the manifest and figure out whether
        // all densities are expected to be needed?
        // Note: ldpi is probably not needed; it has very little usage
        // (about 2%; http://developer.android.com/resources/dashboard/screens.html)
        // TODO: Use the matrix to check out if we can eliminate densities based
        // on the target screens?

        Set<String> definedDensities = new HashSet<String>();
        for (File f : folderToNames.keySet()) {
            definedDensities.add(f.getName());
        }

        // Look for missing folders -- if you define say drawable-mdpi then you
        // should also define -hdpi and -xhdpi.

        List<String> missing = new ArrayList<String>();
        for (String density : REQUIRED_DENSITIES) {
            if (!definedDensities.contains(density)) {
                missing.add(density);
            }
        }
        if (missing.size() > 0) {
            context.client.report(context,
                ICON_DENSITIES,
                null /* location */,
                String.format("Missing density variation folders in %1$s: %2$s",
                        context.project.getDisplayPath(res),
                        LintUtils.formatList(missing, missing.size())),
                null);
        }

        // Look for folders missing some of the specific assets
        Set<String> allNames = new HashSet<String>();
        for (Set<String> n : folderToNames.values()) {
            allNames.addAll(n);
        }
        for (Map.Entry<File, Set<String>> entry : folderToNames.entrySet()) {
            Set<String> names = entry.getValue();
            if (names.size() != allNames.size()) {
                List<String> delta =
                        new ArrayList<String>(difference(allNames, names));
                Collections.sort(delta);
                File file = entry.getKey();
                String foundIn = "";
                if (delta.size() == 1) {
                    // Produce list of where the icon is actually defined
                    List<String> defined = new ArrayList<String>();
                    String name = delta.get(0);
                    for (Map.Entry<File, Set<String>> e : folderToNames.entrySet()) {
                        if (e.getValue().contains(name)) {
                            defined.add(e.getKey().getName());
                        }
                    }
                    if (defined.size() > 0) {
                        foundIn = String.format(" (found in %1$s)",
                                LintUtils.formatList(defined, 5));
                    }
                }

                context.client.report(context,
                        ICON_DENSITIES,
                        new Location(file, null, null),
                        String.format(
                                "Missing the following drawables in %1$s: %2$s%3$s",
                                file.getName(),
                                LintUtils.formatList(delta, 5),
                                foundIn),
                        null);
            }
        }
    }

    private void checkDrawableDir(Context context, File folder, File[] files,
            Map<File, Dimension> dipSizes) {
        if (folder.getName().equals(DRAWABLE_FOLDER)
                && context.configuration.isEnabled(ICON_LOCATION)) {
            for (File file : files) {
                String name = file.getName();
                if (name.endsWith(DOT_XML)) {
                    // pass - most common case, avoids checking other extensions
                } else if (endsWith(name, DOT_PNG)
                        || endsWith(name, DOT_JPG)
                        || endsWith(name, DOT_GIF)) {
                    context.client.report(context,
                        ICON_LOCATION,
                        new Location(file, null, null),
                        String.format("Found bitmap drawable res/drawable/%1$s in " +
                                "densityless folder",
                                file.getName()),
                        null);
                }
            }
        }

        if (context.configuration.isEnabled(GIF_USAGE)) {
            for (File file : files) {
                String name = file.getName();
                if (endsWith(name, DOT_GIF)) {
                    context.client.report(context,
                            GIF_USAGE,
                            new Location(file, null, null),
                            "Using the .gif format for bitmaps is discouraged",
                            null);
                }
            }
        }

        // Check icon sizes
        if (context.configuration.isEnabled(ICON_EXPECTED_SIZE)) {
            checkExpectedSizes(context, folder, files);
        }

        if (dipSizes != null) {
            for (File file : files) {
                // TODO: Combine this check with the check for expected sizes such that
                // I don't check file sizes twice!
                String fileName = file.getName();
                // Only scan .png files (except 9-patch png's) and jpg files
                if (endsWith(fileName, DOT_PNG) && !endsWith(fileName, DOT_9PNG) ||
                        endsWith(fileName, DOT_JPG)) {
                    Dimension size = getSize(file);
                    dipSizes.put(file, size);
                }
            }
        }
    }

    private void checkExpectedSizes(Context context, File folder, File[] files) {
        String folderName = folder.getName();

        int folderVersion = -1;
        String[] qualifiers = folderName.split("-"); //$NON-NLS-1$
        for (String qualifier : qualifiers) {
            if (qualifier.startsWith("v")) {
                Matcher matcher = VERSION_PATTERN.matcher(qualifier);
                if (matcher.matches()) {
                    folderVersion = Integer.parseInt(matcher.group(1));
                }
            }
        }

        for (File file : files) {
            String name = file.getName();

            // TODO: Look up exact app icon from the manifest rather than simply relying on
            // the naming conventions described here:
            //  http://developer.android.com/guide/practices/ui_guidelines/icon_design.html#design-tips
            // See if we can figure out other types of icons from usage too.

            String baseName = name;
            int index = baseName.indexOf('.');
            if (index != -1) {
                baseName = baseName.substring(0, index);
            }

            if (baseName.equals(mApplicationIcon) || name.startsWith("ic_launcher")) { //$NON-NLS-1$
                // Launcher icons
                checkSize(context, folderName, file, 48, 48, true /*exact*/);
            } else if (name.startsWith("ic_action_")) { //$NON-NLS-1$
                // Action Bar
                checkSize(context, folderName, file, 24, 24, true /*exact*/);
            } else if (name.startsWith("ic_dialog_")) { //$NON-NLS-1$
                // Action Bar
                checkSize(context, folderName, file, 32, 32, true /*exact*/);
            } else if (name.startsWith("ic_tab_")) { //$NON-NLS-1$
                // Tab icons
                checkSize(context, folderName, file, 32, 32, true /*exact*/);
            } else if (name.startsWith("ic_stat_")) { //$NON-NLS-1$
                // Notification icons

                if (isAndroid30(context, folderVersion)) {
                    checkSize(context, folderName, file, 24, 24, true /*exact*/);
                } else if (isAndroid23(context, folderVersion)) {
                    checkSize(context, folderName, file, 16, 25, false /*exact*/);
                } else {
                    // Android 2.2 or earlier
                    // TODO: Should this be done for each folder size?
                    checkSize(context, folderName, file, 25, 25, true /*exact*/);
                }
            } else if (name.startsWith("ic_menu_")) { //$NON-NLS-1$
                // Menu icons (<=2.3 only: Replaced by action bar icons (ic_action_ in 3.0).
                if (isAndroid23(context, folderVersion)) {
                    // The icon should be 32x32 inside the transparent image; should
                    // we check that this is mostly the case (a few pixels are allowed to
                    // overlap for anti-aliasing etc)
                    checkSize(context, folderName, file, 48, 48, true /*exact*/);
                } else {
                    // Android 2.2 or earlier
                    // TODO: Should this be done for each folder size?
                    checkSize(context, folderName, file, 48, 48, true /*exact*/);
                }
            }
            // TODO: ListView icons?
        }
    }

    /**
     * Is this drawable folder for an Android 3.0 drawable? This will be the
     * case if it specifies -v11+, or if the minimum SDK version declared in the
     * manifest is at least 11.
     */
    private boolean isAndroid30(Context context, int folderVersion) {
        return folderVersion >= 11 || mMinSdk >= 11;
    }

    /**
     * Is this drawable folder for an Android 2.3 drawable? This will be the
     * case if it specifies -v9 or -v10, or if the minimum SDK version declared in the
     * manifest is 9 or 10 (and it does not specify some higher version like -v11
     */
    private boolean isAndroid23(Context context, int folderVersion) {
        if (isAndroid30(context, folderVersion)) {
            return false;
        }

        return folderVersion == 9 || folderVersion == 10 || mMinSdk == 9 || mMinSdk == 10;
    }

    private float getMdpiScalingFactor(String folderName) {
        // Can't do startsWith(DRAWABLE_MDPI) because the folder could
        // be something like "drawable-sw600dp-mdpi".
        if (folderName.contains("-mdpi")) {            //$NON-NLS-1$
            return 1.0f;
        } else if (folderName.contains("-hdpi")) {     //$NON-NLS-1$
            return 1.5f;
        } else if (folderName.contains("-xhdpi")) {    //$NON-NLS-1$
            return 2.0f;
        } else if (folderName.contains("-ldpi")) {     //$NON-NLS-1$
            return 0.75f;
        } else {
            return 0f;
        }
    }

    private void checkSize(Context context, String folderName, File file,
            int mdpiWidth, int mdpiHeight, boolean exactMatch) {
        String fileName = file.getName();
        // Only scan .png files (except 9-patch png's) and jpg files
        if (!((endsWith(fileName, DOT_PNG) && !endsWith(fileName, DOT_9PNG)) ||
                endsWith(fileName, DOT_JPG))) {
            return;
        }

        int width = -1;
        int height = -1;
        // Use 3:4:6:8 scaling ratio to look up the other expected sizes
        if (folderName.startsWith(DRAWABLE_MDPI)) {
            width = mdpiWidth;
            height = mdpiHeight;
        } else if (folderName.startsWith(DRAWABLE_HDPI)) {
            // Perform math using floating point; if we just do
            //   width = mdpiWidth * 3 / 2;
            // then for mdpiWidth = 25 (as in notification icons on pre-GB) we end up
            // with width = 37, instead of 38 (with floating point rounding we get 37.5 = 38)
            width = Math.round(mdpiWidth * 3.f / 2);
            height = Math.round(mdpiHeight * 3f / 2);
        } else if (folderName.startsWith(DRAWABLE_XHDPI)) {
            width = mdpiWidth * 2;
            height = mdpiHeight * 2;
        } else if (folderName.startsWith(DRAWABLE_LDPI)) {
            width = Math.round(mdpiWidth * 3f / 4);
            height = Math.round(mdpiHeight * 3f / 4);
        } else {
            return;
        }

        Dimension size = getSize(file);
        if (size != null) {
            if (exactMatch && size.width != width || size.height != height) {
                context.client.report(context,
                        ICON_EXPECTED_SIZE,
                    new Location(file, null, null),
                    String.format(
                        "Incorrect icon size for %1$s: expected %2$dx%3$d, but was %4$dx%5$d",
                        folderName + File.separator + file.getName(),
                        width, height, size.width, size.height),
                    null);
            } else if (!exactMatch && size.width > width || size.height > height) {
                context.client.report(context,
                        ICON_EXPECTED_SIZE,
                    new Location(file, null, null),
                    String.format(
                        "Incorrect icon size for %1$s: icon size should be at most %2$dx%3$d, but was %4$dx%5$d",
                        folderName + File.separator + file.getName(),
                        width, height, size.width, size.height),
                    null);
            }
        }
    }

    private Dimension getSize(File file) {
        try {
            ImageInputStream input = ImageIO.createImageInputStream(file);
            if (input != null) {
                try {
                    Iterator<ImageReader> readers = ImageIO.getImageReaders(input);
                    if (readers.hasNext()) {
                        ImageReader reader = readers.next();
                        try {
                            reader.setInput(input);
                            return new Dimension(reader.getWidth(0), reader.getHeight(0));
                        } finally {
                            reader.dispose();
                        }
                    }
                } finally {
                    if (input != null) {
                        input.close();
                    }
                }
            }

            // Fallback: read the image using the normal means
            BufferedImage image = ImageIO.read(file);
            if (image != null) {
                return new Dimension(image.getWidth(), image.getHeight());
            } else {
                return null;
            }
        } catch (IOException e) {
            // Pass -- we can't handle all image types, warn about those we can
            return null;
        }
    }

    // XML detector: Skim manifest

    @Override
    public boolean appliesTo(Context context, File file) {
        return file.getName().equals(ANDROID_MANIFEST_XML);
    }

    @Override
    public Collection<String> getApplicableElements() {
        return Arrays.asList(new String[] {
            TAG_APPLICATION,
            TAG_USES_SDK,
        });
    }

    @Override
    public void visitElement(Context context, Element element) {
        if (element.getTagName().equals(TAG_USES_SDK)) {
            String minSdk = null;
            if (element.hasAttributeNS(ANDROID_URI, ATTR_MIN_SDK_VERSION)) {
                minSdk = element.getAttributeNS(ANDROID_URI, ATTR_MIN_SDK_VERSION);
            }
            if (minSdk != null) {
                try {
                    mMinSdk = Integer.valueOf(minSdk);
                } catch (NumberFormatException e) {
                    mMinSdk = -1;
                }
            }
        } else {
            assert element.getTagName().equals(TAG_APPLICATION);
            mApplicationIcon = element.getAttributeNS(ANDROID_URI, ATTR_ICON);
            if (mApplicationIcon.startsWith(DRAWABLE_RESOURCE_PREFIX)) {
                mApplicationIcon = mApplicationIcon.substring(DRAWABLE_RESOURCE_PREFIX.length());
            }
        }
    }
}
